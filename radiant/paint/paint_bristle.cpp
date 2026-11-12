// bristle trails (PE10), after Strassmann's hairy brushes: each bristle follows the brush with
// its own lag, draws a thin anti-aliased trail of the ink it carries, spends its load on what it
// deposits and mixes in the canvas under its tip. Rows run in path order and bristles in index
// order within a row, so a path split across calls draws exactly what one call draws.
#include "paint_wet_internal.hpp"
#include "lib/mem.h"

static const int32_t PAINT_MAX_BRISTLES = 4096;
static const float PAINT_COORD_LIMIT = 1.0e9f;

static bool paint_unit(float v) { return v >= 0.0f && v <= 1.0f; }

static bool paint_bristle_state_valid(const float* state, int32_t bristles, int32_t columns, int32_t bands, const char** reason, bool wet = false) {
    if (!state || bristles < 1 || bristles > PAINT_MAX_BRISTLES) { *reason = "bristle state needs 1 to 4096 bristles"; return false; }
    if (columns != paint_bristle_columns(bands)) { *reason = "bristle state has the wrong column count for its layer"; return false; }
    for (int32_t j = 0; j < bristles; j++) {
        const float* b = state + (int64_t)j * columns;
        if (!(b[PAINT_BRISTLE_OX] >= -4.0f && b[PAINT_BRISTLE_OX] <= 4.0f && b[PAINT_BRISTLE_OY] >= -4.0f && b[PAINT_BRISTLE_OY] <= 4.0f)) {
            *reason = "bristle offsets must be in [-4, 4] radii"; return false;
        }
        if (!(b[PAINT_BRISTLE_STIFFNESS] >= 0.0f && b[PAINT_BRISTLE_STIFFNESS] <= 0.99f)) { *reason = "bristle stiffness must be in [0, 0.99]"; return false; }
        if (!(b[PAINT_BRISTLE_WIDTH] > 0.0f && b[PAINT_BRISTLE_WIDTH] <= 64.0f)) { *reason = "bristle width must be in (0, 64] pixels"; return false; }
        if (!(b[PAINT_BRISTLE_LOAD] >= 0.0f && b[PAINT_BRISTLE_LOAD] <= 1.0e6f)) { *reason = "bristle load must be in [0, 1000000]"; return false; }
        if (!(fabsf(b[PAINT_BRISTLE_X]) <= PAINT_COORD_LIMIT && fabsf(b[PAINT_BRISTLE_Y]) <= PAINT_COORD_LIMIT)) {
            *reason = "bristle positions must be finite and bounded"; return false;
        }
        if (b[PAINT_BRISTLE_PLACED] != 0.0f && b[PAINT_BRISTLE_PLACED] != 1.0f) { *reason = "bristle placed flag must be 0 or 1"; return false; }
        for (int32_t k = PAINT_BRISTLE_INK; k < columns; k++)
            if (!(wet ? b[k] >= 0.0f && b[k] <= 64.0f : bands > 0 ? b[k] >= 0.0f && b[k] <= 1.0e6f : paint_unit(b[k]))) {
                *reason = "bristle ink must be a colour in [0, 1] (rgba) or film in [0, 1000000] (pigment)"; return false;
            }
        if (wet && b[PAINT_BRISTLE_INK + 2] != 0.0f) { *reason = "wet bristle reserved ink field must be zero"; return false; }
    }
    return true;
}

// a path row: a dab row whose geometry and strength the bristles follow (see paint_bristles).
static bool paint_path_row(const float* row, int32_t width, int32_t height, const PaintBristleParams* params,
        PaintDab* dab, const char** reason) {
    PaintDabParams decode = {};
    decode.work_budget = params->work_budget;
    decode.paper = params->paper;
    if (!paint_dab_decode(row, width, height, &decode, dab, reason)) return false;
    bool shifted = false;
    for (int i = 0; i < 5; i++) shifted |= dab->shift[i] != 0.0f;
    if (dab->lock_alpha != 0.0f || dab->colorize != 0.0f || dab->posterize != 0.0f || dab->blend != PAINT_BLEND_NORMAL || shifted) {
        *reason = "a bristle path takes no lock alpha, colorize, posterize, blend mode or colour shift"; return false;
    }
    return true;
}

static uint64_t paint_rect_area(PaintRect r) {
    return paint_rect_empty(r) ? 0 : (uint64_t)(r.x1 - r.x0) * (uint64_t)(r.y1 - r.y0);
}

enum PaintBristlePass { PAINT_BRISTLE_BOUNDS, PAINT_BRISTLE_CHECK, PAINT_BRISTLE_DRAW };

struct PaintBristleCall {
    const float* path; int64_t count; int32_t width, height, tile_size;
    float* state; int32_t bristles, columns;
    const PaintBristleParams* params;
    PaintTiles* target;          // DRAW and CHECK only
    PaintWetSheet* wet = nullptr;
    PaintWetSheet64* wet64 = nullptr;
    int32_t channel = 0;
};

template<typename T> static void paint_wet_bristle_pickup(PaintWetSheetT<T>* sheet, int32_t channel,
        float* b, float x, float y, float rate) {
    int32_t px = (int32_t)floorf(x), py = (int32_t)floorf(y);  // INT_CAST_OK: canvas pixel of the tip
    if (rate <= 0 || px < 0 || py < 0 || px >= sheet->width || py >= sheet->height) return;
    int64_t n = (int64_t)sheet->width * sheet->height, at = (int64_t)py * sheet->width + px;
    float* ink = b + PAINT_BRISTLE_INK;
    // A wet bristle carries the selected pigment and water; other pigments stay on the sheet.
    ink[0] += rate * (paint_clamp01((float)sheet->cells[(8 + channel)*n + at]) - ink[0]);
    ink[1] += rate * (paint_clamp01((float)sheet->cells[n + at]) - ink[1]);
}

// mixes the canvas under a tip into a bristle's ink, before the bristle draws there.
static void paint_bristle_pickup(PaintTiles* target, float* b, float x, float y, float rate) {
    int32_t px = (int32_t)floorf(x), py = (int32_t)floorf(y);  // INT_CAST_OK: canvas pixel of the tip
    if (rate <= 0.0f || px < 0 || py < 0 || px >= target->width || py >= target->height) return;
    const PaintView* view = paint_tile(target, px / target->tile_size, py / target->tile_size);
    if (!view->data) return;
    const float* p = (const float*)view->data + (int64_t)(py % target->tile_size) * view->row_stride +
        (int64_t)(px % target->tile_size) * view->pixel_stride;
    float* ink = b + PAINT_BRISTLE_INK;
    if (target->format == PAINT_FORMAT_RGBA) {
        if (p[3] <= 0.0f) return;
        float k = rate * paint_clamp01(p[3]);
        for (int c = 0; c < 3; c++) ink[c] += k * (paint_clamp01(p[c] / p[3]) - ink[c]);
        return;
    }
    // pigment: the bristle takes up the film's composition at its own paint body.
    int32_t bands = target->bands;
    float t = p[paint_pigment_t(bands)], body = ink[2 * bands];
    if (t <= 1.0e-6f || body <= 0.0f) return;
    float k = rate * paint_clamp01(t);
    for (int32_t f = 0; f < 2 * bands; f++) ink[f] += k * (p[f] / t * body - ink[f]);
}

// one trail segment, without its end cap so consecutive segments do not double their joints.
static void paint_bristle_segment(const PaintBristleCall* call, const PaintDab* row, const float* b, float x0, float y0,
        float x1, float y1, float strength, PaintRect box, const PaintView* paper) {
    float dx = x1 - x0, dy = y1 - y0, length = sqrtf(dx * dx + dy * dy);
    float ux = dx / length, uy = dy / length, half = 0.5f * b[PAINT_BRISTLE_WIDTH];
    PaintTiles* target = call->target;
    int32_t size = target ? target->tile_size : 0, bands = target ? target->bands : 0, fields = paint_film_fields(bands);
    PaintInk ink = {{b[PAINT_BRISTLE_INK], b[PAINT_BRISTLE_INK + 1], b[PAINT_BRISTLE_INK + 2]}, row->eraser,
                    0.0f, 0.0f, 0.0f, 1.0f, PAINT_BLEND_NORMAL};
    for (int32_t y = box.y0; y < box.y1; y++) for (int32_t x = box.x0; x < box.x1; x++) {
        float px = (float)x + 0.5f - x0, py = (float)y + 0.5f - y0, along = px * ux + py * uy;
        if (along < 0.0f || along >= length) continue;
        float across = fabsf(px * uy - py * ux), c;
        if (row->aa > 0.0f) c = paint_clamp01((half + 0.5f * row->aa - across) / row->aa);
        else c = across <= half ? 1.0f : 0.0f;
        if (c <= 0.0f) continue;
        float a = paint_grained(row, paper, c, x, y) * strength;
        if (a <= 0.0f) continue;
        if (a > 1.0f) a = 1.0f;
        if (call->wet || call->wet64) {
            float mask = paint_grained(row, paper, c, x, y);
            int64_t at = (int64_t)y * call->width + x;
            if (call->wet) paint_wet_pixel(call->wet, call->channel, at, mask, a, b[PAINT_BRISTLE_INK], b[PAINT_BRISTLE_INK+1]);
            else paint_wet_pixel(call->wet64, call->channel, at, mask, a, b[PAINT_BRISTLE_INK], b[PAINT_BRISTLE_INK+1]);
            continue;
        }
        PaintView* view = paint_tile(target, x / size, y / size);
        float* p = (float*)view->data + (int64_t)(y % size) * view->row_stride + (int64_t)(x % size) * view->pixel_stride;
        if (target->format == PAINT_FORMAT_RGBA) paint_apply(p, &ink, a);
        else paint_film_apply(p, fields, a, row->eraser, 0.0f, nullptr, b + PAINT_BRISTLE_INK);
    }
}

// Runs the path once. BOUNDS charges each segment's box and marks its tiles; CHECK confirms the
// tiles are supplied; DRAW picks up, draws, depletes and updates the state. Geometry never reads
// the load, so the three passes agree on every box.
static PaintResult paint_bristle_pass(const PaintBristleCall* call, PaintBristlePass pass, uint8_t* touched) {
    const char* reason = nullptr;
    int32_t tiles_x = (call->width + call->tile_size - 1) / call->tile_size;
    float* positions = nullptr;
    if (pass != PAINT_BRISTLE_DRAW) {
        // the geometry passes move a scratch copy of the tips
        positions = (float*)mem_calloc((size_t)call->bristles * 3, sizeof(float), MEM_CAT_RENDER);
        if (!positions) return paint_fail(PAINT_ERROR_ARGUMENT, "cannot allocate bristle scratch");
        for (int32_t j = 0; j < call->bristles; j++) {
            const float* b = call->state + (int64_t)j * call->columns;
            positions[j * 3] = b[PAINT_BRISTLE_X]; positions[j * 3 + 1] = b[PAINT_BRISTLE_Y]; positions[j * 3 + 2] = b[PAINT_BRISTLE_PLACED];
        }
    }
    PaintRect dirty = {0, 0, 0, 0};
    uint64_t work = 0;
    PaintResult result = paint_ok(dirty, 0);
    for (int64_t i = 0; i < call->count && result.status == PAINT_OK; i++) {
        PaintDab row;
        if (!paint_path_row(call->path + i * PAINT_DAB_COLUMNS, call->width, call->height, call->params, &row, &reason)) {
            result = paint_fail(PAINT_ERROR_ARGUMENT, reason); break;
        }
        if ((call->wet || call->wet64) && row.eraser != 1.0f) {
            result = paint_fail(PAINT_ERROR_ARGUMENT, "wet bristles cannot erase"); break;
        }
        for (int32_t j = 0; j < call->bristles; j++) {
            float* b = call->state + (int64_t)j * call->columns;
            float* tip = positions ? positions + j * 3 : nullptr;
            float x = tip ? tip[0] : b[PAINT_BRISTLE_X], y = tip ? tip[1] : b[PAINT_BRISTLE_Y];
            bool placed = (tip ? tip[2] : b[PAINT_BRISTLE_PLACED]) != 0.0f;
            float ox = b[PAINT_BRISTLE_OX] * row.radius, oy = b[PAINT_BRISTLE_OY] * row.radius / row.aspect;
            float tx = row.x + ox * row.cos_angle - oy * row.sin_angle, ty = row.y + ox * row.sin_angle + oy * row.cos_angle;
            float nx = tx, ny = ty;
            if (placed) { float follow = 1.0f - b[PAINT_BRISTLE_STIFFNESS]; nx = x + follow * (tx - x); ny = y + follow * (ty - y); }
            float length = placed ? sqrtf((nx - x) * (nx - x) + (ny - y) * (ny - y)) : 0.0f;
            if (tip) { tip[0] = nx; tip[1] = ny; tip[2] = 1.0f; }
            if (length > 1.0e-6f) {
                double reach = 0.5 * b[PAINT_BRISTLE_WIDTH] + row.aa;
                double left = floor(fmin(x, nx) - reach), top = floor(fmin(y, ny) - reach);
                double right = ceil(fmax(x, nx) + reach), bottom = ceil(fmax(y, ny) + reach);
                PaintRect box = {(int32_t)fmax(0.0, left), (int32_t)fmax(0.0, top),  // INT_CAST_OK: clipped pixel bounds
                                 (int32_t)fmin((double)call->width, right), (int32_t)fmin((double)call->height, bottom)};
                if (!paint_rect_empty(box)) {
                    int32_t size = call->tile_size;
                    if (pass == PAINT_BRISTLE_BOUNDS) {
                        uint64_t area = paint_rect_area(box);
                        if (area > call->params->work_budget - work) { result = paint_fail(PAINT_ERROR_BUDGET, "bristles exceed the bounded work budget"); break; }
                        work += area;
                        dirty = paint_rect_union(dirty, box);
                        if (touched)
                            for (int32_t ty0 = box.y0 / size; ty0 <= (box.y1 - 1) / size; ty0++)
                                for (int32_t tx0 = box.x0 / size; tx0 <= (box.x1 - 1) / size; tx0++) touched[(int64_t)ty0 * tiles_x + tx0] = 1;
                    } else if (pass == PAINT_BRISTLE_CHECK) {
                        for (int32_t ty0 = box.y0 / size; ty0 <= (box.y1 - 1) / size; ty0++)
                            for (int32_t tx0 = box.x0 / size; tx0 <= (box.x1 - 1) / size; tx0++)
                                if (!paint_tile(call->target, tx0, ty0)->data) result = paint_fail(PAINT_ERROR_ARGUMENT, "a tile the bristles write was not supplied");
                        if (result.status != PAINT_OK) break;
                    } else {
                        float strength = row.opacity * row.flow * fminf(1.0f, b[PAINT_BRISTLE_LOAD]);
                        if (strength > 0.0f) {
                            float rate = call->params->pickup * fminf(1.0f, length);
                            if (call->wet) paint_wet_bristle_pickup(call->wet, call->channel, b, nx, ny, rate);
                            else if (call->wet64) paint_wet_bristle_pickup(call->wet64, call->channel, b, nx, ny, rate);
                            else paint_bristle_pickup(call->target, b, nx, ny, rate);
                            paint_bristle_segment(call, &row, b, x, y, nx, ny, strength, box, call->params->paper);
                            b[PAINT_BRISTLE_LOAD] = fmaxf(0.0f, b[PAINT_BRISTLE_LOAD] - call->params->depletion * length * strength);
                        }
                    }
                }
            }
            if (!tip) { b[PAINT_BRISTLE_X] = nx; b[PAINT_BRISTLE_Y] = ny; b[PAINT_BRISTLE_PLACED] = 1.0f; }
        }
    }
    if (positions) mem_free(positions);
    if (result.status != PAINT_OK) return result;
    return paint_ok(dirty, work);
}

static bool paint_bristle_params_valid(const PaintBristleParams* params, const char** reason) {
    if (!params) { *reason = "bristle parameters are missing"; return false; }
    if (!(params->depletion >= 0.0f && params->depletion <= 1000.0f)) { *reason = "bristle depletion must be in [0, 1000]"; return false; }
    if (!paint_unit(params->pickup)) { *reason = "bristle pickup must be in [0, 1]"; return false; }
    if (params->pigment_bands < 0 || params->pigment_bands > PAINT_MAX_BANDS) { *reason = "pigment layers have 1 to 8 bands"; return false; }
    if (params->paper && !(params->paper->data && params->paper->elem == PAINT_ELEM_F32 && params->paper->channels == 1 &&
        params->paper->width > 0 && params->paper->height > 0 && params->paper->row_stride >= (int64_t)params->paper->width * params->paper->pixel_stride)) {
        *reason = "paper must be a f32 (h, w) plane"; return false;
    }
    return true;
}

PaintResult paint_bristle_bounds(const float* path, int64_t count, int32_t columns, const float* state,
        int32_t bristles, int32_t state_columns, int32_t width, int32_t height, int32_t tile_size,
        const PaintBristleParams* params, uint8_t* touched) {
    const char* reason = nullptr;
    if (columns != PAINT_DAB_COLUMNS) return paint_fail(PAINT_ERROR_ARGUMENT, "a bristle path must have 34 columns (dab layout v1)");
    if (count < 0 || (count > 0 && !path)) return paint_fail(PAINT_ERROR_ARGUMENT, "bristle path is missing");
    if (width <= 0 || height <= 0 || tile_size <= 0) return paint_fail(PAINT_ERROR_ARGUMENT, "paint extent is invalid");
    if (!paint_bristle_params_valid(params, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (!paint_bristle_state_valid(state, bristles, state_columns, params->pigment_bands, &reason))
        return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    PaintBristleCall call = {path, count, width, height, tile_size, (float*)state, bristles, state_columns, params, nullptr};
    return paint_bristle_pass(&call, PAINT_BRISTLE_BOUNDS, touched);
}

PaintResult paint_bristles(PaintTiles* target, const float* path, int64_t count, int32_t columns, float* state,
        int32_t bristles, int32_t state_columns, const PaintBristleParams* params) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(target, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (target->format == PAINT_FORMAT_COVERAGE) return paint_fail(PAINT_ERROR_ARGUMENT, "bristles need an rgba or pigment layer");
    if (!params || (target->format == PAINT_FORMAT_PIGMENT) != (params->pigment_bands > 0) ||
        (params->pigment_bands > 0 && params->pigment_bands != target->bands))
        return paint_fail(PAINT_ERROR_ARGUMENT, "pigment bands of the bristle parameters must match the target layer");
    PaintResult bounds = paint_bristle_bounds(path, count, columns, state, bristles, state_columns, target->width,
        target->height, target->tile_size, params, nullptr);
    if (bounds.status != PAINT_OK) return bounds;
    // every written tile must have been supplied before the first write (PE3).
    PaintBristleCall call = {path, count, target->width, target->height, target->tile_size, state, bristles, state_columns, params, target};
    PaintResult check = paint_bristle_pass(&call, PAINT_BRISTLE_CHECK, nullptr);
    if (check.status != PAINT_OK) return check;
    PaintResult drawn = paint_bristle_pass(&call, PAINT_BRISTLE_DRAW, nullptr);
    if (drawn.status != PAINT_OK) return drawn;
    return bounds;
}

template<typename T> static PaintResult paint_wet_bristle_call(PaintWetSheetT<T>* sheet, const float* path, int64_t count,
        int32_t columns, float* state, int32_t bristles, int32_t state_columns, int32_t channel,
        const PaintBristleParams* params, bool apply) {
    const char* reason = nullptr;
    if (!paint_wet_sheet_valid(sheet) || channel < 0 || channel >= sheet->pigments || columns != PAINT_DAB_COLUMNS ||
        count < 0 || (count && !path)) return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet sheet, pigment or bristle path");
    if (!paint_bristle_params_valid(params, &reason) || params->pigment_bands != 0 ||
        !paint_bristle_state_valid(state, bristles, state_columns, 0, &reason, true))
        return paint_fail(PAINT_ERROR_ARGUMENT, reason ? reason : "invalid wet bristle parameters");
    PaintBristleCall call = {path, count, sheet->width, sheet->height, 64, state, bristles, state_columns, params, nullptr};
    // The lane is known at the API boundary; geometry and segment order are shared with PE10.
    if constexpr (sizeof(T) == sizeof(float)) call.wet = (PaintWetSheet*)sheet;
    else call.wet64 = (PaintWetSheet64*)sheet;
    call.channel = channel;
    PaintResult bounds = paint_bristle_pass(&call, PAINT_BRISTLE_BOUNDS, nullptr);
    if (bounds.status != PAINT_OK || !apply) return bounds;
    PaintResult drawn = paint_bristle_pass(&call, PAINT_BRISTLE_DRAW, nullptr);
    if (drawn.status != PAINT_OK) return drawn;
    paint_wet_expand(sheet, bounds.dirty);
    return bounds;
}

PaintResult paint_wet_bristles_bounds(PaintWetSheet* s, const float* path, int64_t n, int32_t cols,
        float* state, int32_t bristles, int32_t state_cols, int32_t channel, const PaintBristleParams* p) {
    return paint_wet_bristle_call(s, path, n, cols, state, bristles, state_cols, channel, p, false);
}
PaintResult paint_wet_bristles(PaintWetSheet* s, const float* path, int64_t n, int32_t cols,
        float* state, int32_t bristles, int32_t state_cols, int32_t channel, const PaintBristleParams* p) {
    return paint_wet_bristle_call(s, path, n, cols, state, bristles, state_cols, channel, p, true);
}
PaintResult paint_wet_bristles_bounds64(PaintWetSheet64* s, const float* path, int64_t n, int32_t cols,
        float* state, int32_t bristles, int32_t state_cols, int32_t channel, const PaintBristleParams* p) {
    return paint_wet_bristle_call(s, path, n, cols, state, bristles, state_cols, channel, p, false);
}
PaintResult paint_wet_bristles64(PaintWetSheet64* s, const float* path, int64_t n, int32_t cols,
        float* state, int32_t bristles, int32_t state_cols, int32_t channel, const PaintBristleParams* p) {
    return paint_wet_bristle_call(s, path, n, cols, state, bristles, state_cols, channel, p, true);
}
