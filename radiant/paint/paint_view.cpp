// view validation, dab decoding and the bounds query (PE3, PE11).
#include "paint_internal.hpp"

static const float PAINT_COORD_LIMIT = 1.0e9f;
static const float PAINT_RADIUS_LIMIT = 8192.0f;
static const float PAINT_ASPECT_LIMIT = 100.0f;
static const float PAINT_AA_LIMIT = 8.0f;
static const float PAINT_SMUDGE_RADIUS_LIMIT = 64.0f;

static bool paint_view_valid(const PaintView* view, int32_t size, int32_t channels, PaintElem elem) {
    if (!view->data) return true;
    if (view->elem != elem || view->width != size || view->height != size || view->channels != channels) return false;
    if (view->pixel_stride < channels || view->row_stride < (int64_t)size * view->pixel_stride) return false;
    return true;
}

// a single-channel f32 plane: a bitmap tip or the paper.
static bool paint_plane_valid(const PaintView* view) {
    return view && view->data && view->elem == PAINT_ELEM_F32 && view->channels == 1 && view->width > 0 &&
        view->height > 0 && view->width <= 4096 && view->height <= 4096 && view->pixel_stride >= 1 &&
        view->row_stride >= (int64_t)view->width * view->pixel_stride;
}

bool paint_tiles_valid(const PaintTiles* tiles, const char** reason) {
    if (!tiles || !tiles->tiles) { *reason = "paint tiles are missing"; return false; }
    if (tiles->width <= 0 || tiles->height <= 0 || tiles->tile_size <= 0 || tiles->tile_size > 4096) {
        *reason = "paint layer extent or tile size is invalid"; return false;
    }
    int64_t tx = ((int64_t)tiles->width + tiles->tile_size - 1) / tiles->tile_size;
    int64_t ty = ((int64_t)tiles->height + tiles->tile_size - 1) / tiles->tile_size;
    if (tx != tiles->tiles_x || ty != tiles->tiles_y) { *reason = "paint tile grid does not match the extent"; return false; }
    int32_t channels;
    if (tiles->format == PAINT_FORMAT_RGBA) {
        if (tiles->elem != PAINT_ELEM_F32) { *reason = "rgba paint tiles must be f32"; return false; }
        channels = 4;
    } else if (tiles->format == PAINT_FORMAT_COVERAGE) {
        if (tiles->elem != PAINT_ELEM_F32 && tiles->elem != PAINT_ELEM_U8) {
            *reason = "coverage tiles must be u8 or f32"; return false;
        }
        channels = 1;
    } else { *reason = "unknown paint layer format"; return false; }
    for (int64_t i = 0; i < tx * ty; i++)
        if (!paint_view_valid(&tiles->tiles[i], tiles->tile_size, channels, tiles->elem)) {
            *reason = "a paint tile does not match the layer format"; return false;
        }
    return true;
}

static bool paint_params_valid(const PaintDabParams* params, const char** reason) {
    if (!params) { *reason = "paint parameters are missing"; return false; }
    if (params->tip_count < 0 || (params->tip_count > 0 && !params->tips)) { *reason = "paint tips are missing"; return false; }
    for (int32_t i = 0; i < params->tip_count; i++)
        if (!paint_plane_valid(&params->tips[i])) { *reason = "a paint tip must be a f32 (h, w) plane of at most 4096 x 4096"; return false; }
    if (params->paper && !paint_plane_valid(params->paper)) { *reason = "paper must be a f32 (h, w) plane of at most 4096 x 4096"; return false; }
    if (params->smudge_buckets < 0 || (params->smudge_buckets > 0 && !params->smudge)) { *reason = "smudge buckets are missing"; return false; }
    return true;
}

static bool paint_in(float value, float low, float high) { return isfinite(value) && value >= low && value <= high; }
static bool paint_index(float value, int32_t limit) {
    return isfinite(value) && value >= 0.0f && value < (float)limit && floorf(value) == value;
}

static PaintRect paint_box(float x, float y, double extent, int32_t width, int32_t height) {
    double left = floor((double)x - extent), top = floor((double)y - extent);
    double right = ceil((double)x + extent), bottom = ceil((double)y + extent);
    if (left < 0) left = 0; if (top < 0) top = 0;
    if (right > width) right = width; if (bottom > height) bottom = height;
    PaintRect none = {0, 0, 0, 0};
    if (right <= left || bottom <= top) return none;
    PaintRect box = {(int32_t)left, (int32_t)top, (int32_t)right, (int32_t)bottom};  // INT_CAST_OK: clipped pixel bounds
    return box;
}

bool paint_dab_decode(const float* row, int32_t width, int32_t height, const PaintDabParams* params,
        PaintDab* dab, const char** reason) {
    if (!paint_in(row[PAINT_DAB_X], -PAINT_COORD_LIMIT, PAINT_COORD_LIMIT) ||
        !paint_in(row[PAINT_DAB_Y], -PAINT_COORD_LIMIT, PAINT_COORD_LIMIT)) { *reason = "dab position must be finite and bounded"; return false; }
    if (!paint_in(row[PAINT_DAB_RADIUS], 0.0f, PAINT_RADIUS_LIMIT) || row[PAINT_DAB_RADIUS] <= 0.0f) {
        *reason = "dab radius must be in (0, 8192]"; return false;
    }
    if (!paint_in(row[PAINT_DAB_ASPECT], 1.0f, PAINT_ASPECT_LIMIT)) { *reason = "dab aspect must be in [1, 100]"; return false; }
    if (!isfinite(row[PAINT_DAB_ANGLE])) { *reason = "dab angle must be finite"; return false; }
    if (!paint_in(row[PAINT_DAB_AA], 0.0f, PAINT_AA_LIMIT)) { *reason = "dab anti-aliasing must be in [0, 8]"; return false; }
    static const int unit_columns[] = {PAINT_DAB_HARDNESS, PAINT_DAB_OPACITY, PAINT_DAB_FLOW, PAINT_DAB_R, PAINT_DAB_G,
        PAINT_DAB_B, PAINT_DAB_ERASER, PAINT_DAB_LOCK_ALPHA, PAINT_DAB_COLORIZE, PAINT_DAB_POSTERIZE, PAINT_DAB_SMUDGE,
        PAINT_DAB_SMUDGE_LENGTH, PAINT_DAB_GRAIN};
    for (int column : unit_columns)
        if (!paint_in(row[column], 0.0f, 1.0f)) {
            *reason = "dab hardness, opacity, flow, colour, eraser, lock alpha, colorize, posterize, smudge, smudge length and grain must be in [0, 1]";
            return false;
        }
    for (int column = PAINT_DAB_DH; column <= PAINT_DAB_DHSV_S; column++)
        if (!paint_in(row[column], -1.0f, 1.0f)) { *reason = "dab colour shifts must be in [-1, 1]"; return false; }
    if (row[PAINT_DAB_POSTERIZE] > 0.0f && !(paint_index(row[PAINT_DAB_POSTERIZE_LEVELS], 129) && row[PAINT_DAB_POSTERIZE_LEVELS] >= 1.0f)) {
        *reason = "posterize levels must be an integer in [1, 128]"; return false;
    }
    if (!isfinite(row[PAINT_DAB_POSTERIZE_LEVELS])) { *reason = "dab parameters must be finite"; return false; }
    if (!paint_in(row[PAINT_DAB_SMUDGE_RADIUS], 0.0f, PAINT_SMUDGE_RADIUS_LIMIT)) { *reason = "smudge radius factor must be in [0, 64]"; return false; }
    bool pickup = row[PAINT_DAB_SMUDGE_RADIUS] > 0.0f && row[PAINT_DAB_SMUDGE_LENGTH] < 1.0f;
    if (!paint_index(row[PAINT_DAB_SMUDGE_BUCKET], 1 << 16)) { *reason = "smudge bucket must be a non-negative integer"; return false; }
    if ((pickup || row[PAINT_DAB_SMUDGE] > 0.0f) && !paint_index(row[PAINT_DAB_SMUDGE_BUCKET], params ? params->smudge_buckets : 0)) {
        *reason = "a smudging dab needs its smudge bucket"; return false;
    }
    if (row[PAINT_DAB_TIP] != -1.0f && !paint_index(row[PAINT_DAB_TIP], params ? params->tip_count : 0)) {
        *reason = "dab tip must be -1 or the index of a supplied tip"; return false;
    }
    if (row[PAINT_DAB_GRAIN] > 0.0f && !(params && params->paper)) { *reason = "a grained dab needs paper"; return false; }
    // pigment, wet and impasto columns arrive with later media (PE8, PE9); refuse rather than ignore.
    if (row[PAINT_DAB_PIGMENT] != -1.0f || row[PAINT_DAB_AMOUNT] != 0.0f || row[PAINT_DAB_WATER] != 0.0f ||
        row[PAINT_DAB_THICKNESS] != 0.0f) { *reason = "dab uses a medium this engine version does not support"; return false; }
    float profile = row[PAINT_DAB_PROFILE];
    if (profile != (float)PAINT_PROFILE_MYPAINT && profile != (float)PAINT_PROFILE_LINEAR) {
        *reason = "unknown dab falloff profile"; return false;
    }
    if (!paint_index(row[PAINT_DAB_BLEND], PAINT_BLEND_COUNT)) { *reason = "unknown dab blend mode"; return false; }
    dab->x = row[PAINT_DAB_X]; dab->y = row[PAINT_DAB_Y]; dab->radius = row[PAINT_DAB_RADIUS];
    dab->aspect = row[PAINT_DAB_ASPECT];
    dab->cos_angle = cosf(row[PAINT_DAB_ANGLE]); dab->sin_angle = sinf(row[PAINT_DAB_ANGLE]);
    dab->hardness = row[PAINT_DAB_HARDNESS]; dab->aa = row[PAINT_DAB_AA];
    dab->opacity = row[PAINT_DAB_OPACITY]; dab->flow = row[PAINT_DAB_FLOW];
    for (int c = 0; c < 3; c++) dab->color[c] = row[PAINT_DAB_R + c];
    dab->eraser = row[PAINT_DAB_ERASER]; dab->lock_alpha = row[PAINT_DAB_LOCK_ALPHA];
    dab->colorize = row[PAINT_DAB_COLORIZE]; dab->posterize = row[PAINT_DAB_POSTERIZE];
    dab->posterize_levels = row[PAINT_DAB_POSTERIZE_LEVELS];
    dab->smudge = row[PAINT_DAB_SMUDGE]; dab->smudge_length = row[PAINT_DAB_SMUDGE_LENGTH];
    dab->smudge_radius = row[PAINT_DAB_SMUDGE_RADIUS]; dab->grain = row[PAINT_DAB_GRAIN];
    dab->smudge_bucket = (int32_t)row[PAINT_DAB_SMUDGE_BUCKET];  // INT_CAST_OK: validated bucket index
    for (int i = 0; i < 5; i++) dab->shift[i] = row[PAINT_DAB_DH + i];
    dab->tip = row[PAINT_DAB_TIP] >= 0.0f ? &params->tips[(int32_t)row[PAINT_DAB_TIP]] : nullptr;  // INT_CAST_OK: validated tip index
    dab->profile = profile == (float)PAINT_PROFILE_LINEAR ? PAINT_PROFILE_LINEAR : PAINT_PROFILE_MYPAINT;
    dab->blend = (uint8_t)row[PAINT_DAB_BLEND];  // INT_CAST_OK: validated blend index
    // candidate pixels: centres within radius + aa of the dab centre, clipped to the canvas.
    dab->box = paint_box(dab->x, dab->y, (double)dab->radius + dab->aa, width, height);
    PaintRect none = {0, 0, 0, 0};
    dab->pickup = pickup ? paint_box(dab->x, dab->y, fmax(0.5, (double)dab->radius * dab->smudge_radius), width, height) : none;
    return true;
}

static uint64_t paint_area(PaintRect r) {
    return paint_rect_empty(r) ? 0 : (uint64_t)(r.x1 - r.x0) * (uint64_t)(r.y1 - r.y0);
}

PaintResult paint_dab_bounds(const float* dabs, int64_t count, int32_t columns,
        int32_t width, int32_t height, int32_t tile_size, const PaintDabParams* params, uint8_t* touched) {
    const char* reason = nullptr;
    if (columns != PAINT_DAB_COLUMNS) return paint_fail(PAINT_ERROR_ARGUMENT, "dab table must have 34 columns (layout v1)");
    if (count < 0 || (count > 0 && !dabs)) return paint_fail(PAINT_ERROR_ARGUMENT, "dab table is missing");
    if (width <= 0 || height <= 0 || tile_size <= 0) return paint_fail(PAINT_ERROR_ARGUMENT, "paint extent is invalid");
    if (!paint_params_valid(params, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    int32_t tiles_x = (width + tile_size - 1) / tile_size;
    PaintRect dirty = {0, 0, 0, 0};
    uint64_t work = 0;
    for (int64_t i = 0; i < count; i++) {
        PaintDab dab;
        if (!paint_dab_decode(dabs + i * columns, width, height, params, &dab, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
        uint64_t area = paint_area(dab.box) + paint_area(dab.pickup);
        if (area > params->work_budget - work) return paint_fail(PAINT_ERROR_BUDGET, "dabs exceed the bounded work budget");
        work += area;
        if (paint_rect_empty(dab.box)) continue;
        dirty = paint_rect_union(dirty, dab.box);
        if (touched)
            for (int32_t ty = dab.box.y0 / tile_size; ty <= (dab.box.y1 - 1) / tile_size; ty++)
                for (int32_t tx = dab.box.x0 / tile_size; tx <= (dab.box.x1 - 1) / tile_size; tx++)
                    touched[(int64_t)ty * tiles_x + tx] = 1;
    }
    return paint_ok(dirty, work);
}
