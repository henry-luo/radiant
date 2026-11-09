// dab rendering and the wash composite (PE4).
#include "paint_internal.hpp"

// what one dab deposits once its colour is resolved.
struct PaintInk {
    float color[3];
    float target_alpha;          // libmypaint eraser_target_alpha: 1 paints, 0 erases
    float lock, colorize, posterize, levels;
    uint8_t blend;
};

static inline float paint_blend_channel(uint8_t mode, float b, float s) {
    switch (mode) {
    case PAINT_BLEND_MULTIPLY: return b * s;
    case PAINT_BLEND_SCREEN: return b + s - b * s;
    case PAINT_BLEND_OVERLAY: return b <= 0.5f ? 2.0f * b * s : 1.0f - 2.0f * (1.0f - b) * (1.0f - s);
    case PAINT_BLEND_DARKEN: return fminf(b, s);
    case PAINT_BLEND_LIGHTEN: return fmaxf(b, s);
    case PAINT_BLEND_ADD: return fminf(1.0f, b + s);
    default: return s;
    }
}

static inline float paint_lum(const float* c) { return 0.3f * c[0] + 0.59f * c[1] + 0.11f * c[2]; }

// W3C SetLum with ClipColor: the colour's hue and saturation at luminosity l.
static inline void paint_set_lum(const float* c, float l, float* out) {
    float d = l - paint_lum(c);
    for (int i = 0; i < 3; i++) out[i] = c[i] + d;
    float lum = paint_lum(out), mn = fminf(out[0], fminf(out[1], out[2])), mx = fmaxf(out[0], fmaxf(out[1], out[2]));
    for (int i = 0; i < 3; i++) {
        if (mn < 0.0f && lum - mn > 0.0f) out[i] = lum + (out[i] - lum) * lum / (lum - mn);
        if (mx > 1.0f && mx - lum > 0.0f) out[i] = lum + (out[i] - lum) * (1.0f - lum) / (mx - lum);
    }
}

// one premultiplied sample under a dab of strength a: the normal part (with its blend mode)
// moves toward (target_alpha * colour, target_alpha); lock alpha, colorize and posterize recolour
// what is there without adding coverage (libmypaint's dab modes, applied in that order).
static inline void paint_apply(float* p, const PaintInk* ink, float a) {
    float an = a * (1.0f - ink->lock) * (1.0f - ink->colorize) * (1.0f - ink->posterize);
    if (an > 0.0f) {
        float src[3] = {ink->color[0], ink->color[1], ink->color[2]};
        if (ink->blend != PAINT_BLEND_NORMAL && p[3] > 0.0f)
            for (int c = 0; c < 3; c++) {
                float b = paint_clamp01(p[c] / p[3]);
                src[c] = (1.0f - p[3]) * src[c] + p[3] * paint_blend_channel(ink->blend, b, src[c]);
            }
        float keep = 1.0f - an, gain = an * ink->target_alpha;
        for (int c = 0; c < 3; c++) p[c] = gain * src[c] + keep * p[c];
        p[3] = gain + keep * p[3];
    }
    float recolour[3];
    if (ink->lock > 0.0f) {
        float al = a * ink->lock;
        for (int c = 0; c < 3; c++) p[c] = al * ink->color[c] * p[3] + (1.0f - al) * p[c];
    }
    if (ink->colorize > 0.0f && p[3] > 0.0f) {
        float ac = a * ink->colorize, backdrop[3] = {p[0] / p[3], p[1] / p[3], p[2] / p[3]};
        paint_set_lum(ink->color, paint_lum(backdrop), recolour);
        for (int c = 0; c < 3; c++) p[c] = ac * recolour[c] * p[3] + (1.0f - ac) * p[c];
    }
    if (ink->posterize > 0.0f && p[3] > 0.0f) {
        float ap = a * ink->posterize;
        for (int c = 0; c < 3; c++) {
            float level = roundf(paint_clamp01(p[c] / p[3]) * ink->levels) / ink->levels;
            p[c] = ap * level * p[3] + (1.0f - ap) * p[c];
        }
    }
}

static inline float paint_paper(const PaintView* paper, int32_t x, int32_t y) {
    int32_t px = x % paper->width, py = y % paper->height;
    return paint_clamp01(((const float*)paper->data)[(int64_t)py * paper->row_stride + (int64_t)px * paper->pixel_stride]);
}

// renders one dab into the tiles intersecting its box; every such tile is present.
static void paint_render_dab(PaintTiles* target, const PaintDab* dab, const PaintInk* ink, const PaintView* paper) {
    int32_t size = target->tile_size;
    for (int32_t ty = dab->box.y0 / size; ty <= (dab->box.y1 - 1) / size; ty++) {
        int32_t y0 = ty * size > dab->box.y0 ? ty * size : dab->box.y0;
        int32_t y1 = (ty + 1) * size < dab->box.y1 ? (ty + 1) * size : dab->box.y1;
        for (int32_t tx = dab->box.x0 / size; tx <= (dab->box.x1 - 1) / size; tx++) {
            int32_t x0 = tx * size > dab->box.x0 ? tx * size : dab->box.x0;
            int32_t x1 = (tx + 1) * size < dab->box.x1 ? (tx + 1) * size : dab->box.x1;
            PaintView* view = paint_tile(target, tx, ty);
            for (int32_t y = y0; y < y1; y++) {
                float dy = (float)y + 0.5f - dab->y;
                int64_t row = (int64_t)(y - ty * size) * view->row_stride;
                for (int32_t x = x0; x < x1; x++) {
                    float m = paint_dab_mask(dab, (float)x + 0.5f - dab->x, dy);
                    if (m <= 0.0f) continue;
                    // paper grain: the dab reaches peaks fully and valleys by (1 - grain).
                    if (dab->grain > 0.0f) m *= 1.0f - dab->grain + dab->grain * paint_paper(paper, x, y);
                    int64_t at = row + (int64_t)(x - tx * size) * view->pixel_stride;
                    if (target->format == PAINT_FORMAT_RGBA) {
                        float a = m * dab->opacity * dab->flow;
                        if (a > 1.0f) a = 1.0f;
                        if (a > 0.0f) paint_apply((float*)view->data + at, ink, a);
                    } else {
                        float gain = m * dab->flow;
                        if (gain <= 0.0f) continue;
                        if (target->elem == PAINT_ELEM_F32) {
                            float* c = (float*)view->data + at;
                            *c = 1.0f - (1.0f - *c) * (1.0f - gain);
                        } else {
                            uint8_t* c = (uint8_t*)view->data + at;
                            float next = 1.0f - (1.0f - *c / 255.0f) * (1.0f - gain);
                            *c = (uint8_t)lroundf(next * 255.0f);  // INT_CAST_OK: u8 sample quantization
                        }
                    }
                }
            }
        }
    }
}

PaintResult paint_dabs(PaintTiles* target, const float* dabs, int64_t count, int32_t columns, const PaintDabParams* params) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(target, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    PaintResult bounds = paint_dab_bounds(dabs, count, columns, target->width, target->height,
        target->tile_size, params, nullptr);
    if (bounds.status != PAINT_OK) return bounds;
    // every written tile must have been supplied before the first write (PE3).
    for (int64_t i = 0; i < count; i++) {
        PaintDab dab;
        paint_dab_decode(dabs + i * columns, target->width, target->height, params, &dab, &reason);
        if (paint_rect_empty(dab.box)) continue;
        for (int32_t ty = dab.box.y0 / target->tile_size; ty <= (dab.box.y1 - 1) / target->tile_size; ty++)
            for (int32_t tx = dab.box.x0 / target->tile_size; tx <= (dab.box.x1 - 1) / target->tile_size; tx++)
                if (!paint_tile(target, tx, ty)->data) return paint_fail(PAINT_ERROR_ARGUMENT, "a tile the dabs write was not supplied");
    }
    bool rgba = target->format == PAINT_FORMAT_RGBA;
    for (int64_t i = 0; i < count; i++) {
        PaintDab dab;
        paint_dab_decode(dabs + i * columns, target->width, target->height, params, &dab, &reason);
        PaintInk ink = {{dab.color[0], dab.color[1], dab.color[2]}, dab.eraser, dab.lock_alpha, dab.colorize,
                        dab.posterize, dab.posterize_levels, dab.blend};
        // smudge picks up what earlier dabs left, so colour resolves before this dab draws.
        if (rgba) ink.target_alpha = paint_resolve_color(target, &dab, params->smudge, ink.color);
        if (params->resolved) {
            float* out = params->resolved + i * 4;
            out[0] = ink.color[0]; out[1] = ink.color[1]; out[2] = ink.color[2]; out[3] = ink.target_alpha;
        }
        if (!paint_rect_empty(dab.box)) paint_render_dab(target, &dab, &ink, params->paper);
    }
    return bounds;
}

static bool paint_same_grid(const PaintTiles* a, const PaintTiles* b) {
    return a->width == b->width && a->height == b->height && a->tile_size == b->tile_size;
}

PaintResult paint_wash_composite(PaintTiles* target, const PaintTiles* source,
        const PaintTiles* coverage, PaintRect rect, const PaintWashParams* params) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(target, &reason) || !paint_tiles_valid(source, &reason) ||
        !paint_tiles_valid(coverage, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (target->format != PAINT_FORMAT_RGBA || source->format != PAINT_FORMAT_RGBA ||
        coverage->format != PAINT_FORMAT_COVERAGE) return paint_fail(PAINT_ERROR_ARGUMENT, "wash needs rgba layers and a coverage layer");
    if (!paint_same_grid(target, source) || !paint_same_grid(target, coverage))
        return paint_fail(PAINT_ERROR_ARGUMENT, "wash layers must share one tile grid");
    if (!params) return paint_fail(PAINT_ERROR_ARGUMENT, "wash parameters are missing");
    for (int c = 0; c < 3; c++)
        if (!isfinite(params->color[c]) || params->color[c] < 0.0f || params->color[c] > 1.0f)
            return paint_fail(PAINT_ERROR_ARGUMENT, "wash colour must be in [0, 1]");
    if (!(params->eraser >= 0.0f && params->eraser <= 1.0f) || !(params->lock_alpha >= 0.0f && params->lock_alpha <= 1.0f) ||
        !(params->opacity >= 0.0f && params->opacity <= 1.0f)) return paint_fail(PAINT_ERROR_ARGUMENT, "wash eraser, lock alpha and opacity must be in [0, 1]");
    if (rect.x0 < 0) rect.x0 = 0; if (rect.y0 < 0) rect.y0 = 0;
    if (rect.x1 > target->width) rect.x1 = target->width; if (rect.y1 > target->height) rect.y1 = target->height;
    PaintRect none = {0, 0, 0, 0};
    if (paint_rect_empty(rect)) return paint_ok(none, 0);
    int32_t size = target->tile_size;
    for (int32_t ty = rect.y0 / size; ty <= (rect.y1 - 1) / size; ty++)
        for (int32_t tx = rect.x0 / size; tx <= (rect.x1 - 1) / size; tx++)
            if (!paint_tile(target, tx, ty)->data) return paint_fail(PAINT_ERROR_ARGUMENT, "a tile the wash writes was not supplied");
    PaintInk ink = {{params->color[0], params->color[1], params->color[2]}, params->eraser, params->lock_alpha,
                    0.0f, 0.0f, 1.0f, PAINT_BLEND_NORMAL};
    for (int32_t ty = rect.y0 / size; ty <= (rect.y1 - 1) / size; ty++) {
        int32_t y0 = ty * size > rect.y0 ? ty * size : rect.y0, y1 = (ty + 1) * size < rect.y1 ? (ty + 1) * size : rect.y1;
        for (int32_t tx = rect.x0 / size; tx <= (rect.x1 - 1) / size; tx++) {
            int32_t x0 = tx * size > rect.x0 ? tx * size : rect.x0, x1 = (tx + 1) * size < rect.x1 ? (tx + 1) * size : rect.x1;
            PaintView *out = paint_tile(target, tx, ty), *in = paint_tile(source, tx, ty), *cov = paint_tile(coverage, tx, ty);
            for (int32_t y = y0; y < y1; y++) for (int32_t x = x0; x < x1; x++) {
                int32_t lx = x - tx * size, ly = y - ty * size;
                float p[4] = {0.0f, 0.0f, 0.0f, 0.0f};
                if (in->data) {
                    const float* s = (const float*)in->data + (int64_t)ly * in->row_stride + (int64_t)lx * in->pixel_stride;
                    p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = s[3];
                }
                float a = 0.0f;
                if (cov->data) {
                    int64_t at = (int64_t)ly * cov->row_stride + (int64_t)lx * cov->pixel_stride;
                    a = coverage->elem == PAINT_ELEM_F32 ? ((const float*)cov->data)[at] : ((const uint8_t*)cov->data)[at] / 255.0f;
                }
                a *= params->opacity;
                if (a > 0.0f) paint_apply(p, &ink, a > 1.0f ? 1.0f : a);
                float* d = (float*)out->data + (int64_t)ly * out->row_stride + (int64_t)lx * out->pixel_stride;
                d[0] = p[0]; d[1] = p[1]; d[2] = p[2]; d[3] = p[3];
            }
        }
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (uint64_t)(rect.y1 - rect.y0));
}
