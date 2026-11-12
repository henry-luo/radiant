#pragma once
// shared helpers for the paint kernels; not part of the engine's public surface.
#include "paint.hpp"
#include <math.h>

static inline PaintResult paint_fail(PaintStatus status, const char* reason) {
    PaintResult result = {status, reason, {0, 0, 0, 0}, 0};
    return result;
}

static inline PaintResult paint_ok(PaintRect dirty, uint64_t work) {
    PaintResult result = {PAINT_OK, nullptr, dirty, work};
    return result;
}

static inline bool paint_rect_empty(PaintRect rect) { return rect.x1 <= rect.x0 || rect.y1 <= rect.y0; }

static inline PaintRect paint_rect_union(PaintRect a, PaintRect b) {
    if (paint_rect_empty(a)) return b;
    if (paint_rect_empty(b)) return a;
    PaintRect r = {a.x0 < b.x0 ? a.x0 : b.x0, a.y0 < b.y0 ? a.y0 : b.y0,
                   a.x1 > b.x1 ? a.x1 : b.x1, a.y1 > b.y1 ? a.y1 : b.y1};
    return r;
}

static inline PaintView* paint_tile(const PaintTiles* tiles, int32_t tx, int32_t ty) {
    return &tiles->tiles[(int64_t)ty * tiles->tiles_x + tx];
}

// a validated dab row, decoded once so the pixel loops read plain fields.
struct PaintDab {
    float x, y, radius, aspect, cos_angle, sin_angle, hardness, aa, opacity, flow;
    float color[3], eraser, lock_alpha, colorize, posterize, posterize_levels;
    float smudge, smudge_length, smudge_radius, grain;
    float shift[5];              // dh, dl, dhsl_s, dv, dhsv_s
    int32_t smudge_bucket;
    int32_t pigment;             // pigment-table row on a pigment target; -1 otherwise
    float amount, thickness;     // pigment load and paint body per unit coverage (PE8)
    const PaintView* tip;        // null for the procedural mask
    uint8_t profile, blend;
    PaintRect box;               // clipped candidate pixels
    PaintRect pickup;            // clipped smudge footprint; empty when the dab picks up nothing
};

// Decodes and validates one row against `params`; unimplemented features must hold their
// neutral values. Water stays 0 (PE9). On an rgba or coverage target pigment is -1 and amount and
// thickness are 0; on a pigment target the dab names a pigment and uses no lock-alpha, colorize,
// posterize or blend mode.
bool paint_dab_decode(const float* row, int32_t width, int32_t height, const PaintDabParams* params,
    PaintDab* dab, const char** reason);

static inline float paint_clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

// bilinear tip lookup at normalized dab coordinates (u, v) in [-1, 1].
static inline float paint_tip_sample(const PaintView* tip, float u, float v) {
    if (u < -1.0f || u > 1.0f || v < -1.0f || v > 1.0f) return 0.0f;
    float fx = (u + 1.0f) * 0.5f * (float)tip->width - 0.5f, fy = (v + 1.0f) * 0.5f * (float)tip->height - 0.5f;
    int32_t x0 = (int32_t)floorf(fx), y0 = (int32_t)floorf(fy);   // INT_CAST_OK: tip texel index
    float tx = fx - (float)x0, ty = fy - (float)y0, sum = 0.0f;
    for (int32_t j = 0; j < 2; j++) for (int32_t i = 0; i < 2; i++) {
        int32_t sx = x0 + i, sy = y0 + j;
        if (sx < 0 || sy < 0 || sx >= tip->width || sy >= tip->height) continue;
        float w = (i ? tx : 1.0f - tx) * (j ? ty : 1.0f - ty);
        sum += w * ((const float*)tip->data)[(int64_t)sy * tip->row_stride + (int64_t)sx * tip->pixel_stride];
    }
    return paint_clamp01(sum);
}

// Mask value in [0,1] at a pixel centre offset (dx, dy) from the dab centre (PE4).
static inline float paint_dab_mask(const PaintDab* dab, float dx, float dy) {
    float xr = dx * dab->cos_angle + dy * dab->sin_angle;
    float yr = (dy * dab->cos_angle - dx * dab->sin_angle) * dab->aspect;
    // a bitmap tip spans the dab's ellipse box; hardness and profile shape only procedural dabs.
    if (dab->tip) return paint_tip_sample(dab->tip, xr / dab->radius, yr / dab->radius);
    float d2 = (xr * xr + yr * yr) / (dab->radius * dab->radius);
    float edge = 1.0f;
    if (dab->aa > 0.0f) {
        // analytic edge: fade over aa pixels centred on the rim; the profile reads its rim value outside.
        float d_px = sqrtf(d2) * dab->radius;
        edge = (dab->radius + 0.5f * dab->aa - d_px) / dab->aa;
        if (edge <= 0.0f) return 0.0f;
        if (edge > 1.0f) edge = 1.0f;
        if (d2 > 1.0f) d2 = 1.0f;
    } else if (d2 > 1.0f) return 0.0f;
    float h = dab->hardness, f;
    if (dab->profile == PAINT_PROFILE_LINEAR) {
        float d = sqrtf(d2);
        f = d <= h ? 1.0f : (h >= 1.0f ? 0.0f : (1.0f - d) / (1.0f - h));
    } else {
        // libmypaint: falls from 1 to h over [0, h], then linearly to 0 at the rim; h == 0 paints nothing.
        if (h <= 0.0f) return 0.0f;
        f = h >= 1.0f || d2 <= h ? 1.0f - d2 * (1.0f / h - 1.0f) : h * (1.0f - d2) / (1.0f - h);
    }
    if (f <= 0.0f) return 0.0f;
    if (f > 1.0f) f = 1.0f;
    return f * edge;
}

static inline float paint_paper(const PaintView* paper, int32_t x, int32_t y) {
    int32_t px = x % paper->width, py = y % paper->height;
    return paint_clamp01(((const float*)paper->data)[(int64_t)py * paper->row_stride + (int64_t)px * paper->pixel_stride]);
}

// a dab's coverage at a pixel after paper grain: the dab reaches peaks fully and valleys by (1 - grain).
static inline float paint_grained(const PaintDab* dab, const PaintView* paper, float m, int32_t x, int32_t y) {
    return dab->grain > 0.0f ? m * (1.0f - dab->grain + dab->grain * paint_paper(paper, x, y)) : m;
}

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

// a pigment pixel's film fields: tK[B], tS[B] and t.
static inline int32_t paint_film_fields(int32_t bands) { return 2 * bands + 1; }

// one pigment pixel under coverage c (PE8): erasing lifts film, smudging moves it toward the bucket's
// film, and the rest deposits `ink` (paint per unit coverage). Deposit alone is additive.
static inline void paint_film_apply(float* p, int32_t fields, float c, float eraser, float smudge,
        const float* bucket, const float* ink) {
    if (eraser < 1.0f) {
        float keep = 1.0f - c * (1.0f - eraser);
        for (int32_t f = 0; f < fields; f++) p[f] *= keep;
    }
    if (smudge > 0.0f) {
        float keep = 1.0f - c * smudge, gain = c * smudge;
        for (int32_t f = 0; f < fields; f++) p[f] = keep * p[f] + gain * bucket[f];
    }
    float deposit = c * (1.0f - smudge) * eraser;
    if (deposit > 0.0f)
        for (int32_t f = 0; f < fields; f++) p[f] += deposit * ink[f];
}

// renders one dab into a pigment target (PE8): pickup and smudge in film space, then deposit.
void paint_pigment_dab(PaintTiles* target, const PaintDab* dab, const PaintDabParams* params);

// presentation of a pigment layer (PE7 steps 2 and 5); params are validated by paint_present.
PaintResult paint_present_pigment(const PaintTiles* layer, PaintRect rect, PaintView* out, const PaintPresentParams* params);

// sRGB helpers built on lib/color.h; encoding rounds exactly as lround(encode(v) * 255).
float paint_srgb_decode_u8(uint8_t sample);
uint8_t paint_srgb_encode_u8(float linear);

// mask-weighted premultiplied average under a round footprint (libmypaint get_color).
void paint_sample_box(const PaintTiles* layer, float x, float y, float radius, PaintRect box, float out[4]);

// a dab's colour after smudge pickup and mixing and the colour shifts; returns the target
// alpha (libmypaint eraser_target_alpha). Updates the dab's smudge bucket in place.
float paint_resolve_color(const PaintTiles* target, const PaintDab* dab, float* buckets, float color[3]);
