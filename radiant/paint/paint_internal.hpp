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
    const PaintView* tip;        // null for the procedural mask
    uint8_t profile, blend;
    PaintRect box;               // clipped candidate pixels
    PaintRect pickup;            // clipped smudge footprint; empty when the dab picks up nothing
};

// Decodes and validates one row against `params`; unimplemented features must hold their
// neutral values (pigment -1; amount, water and thickness 0).
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

// sRGB helpers built on lib/color.h; encoding rounds exactly as lround(encode(v) * 255).
float paint_srgb_decode_u8(uint8_t sample);
uint8_t paint_srgb_encode_u8(float linear);

// mask-weighted premultiplied average under a round footprint (libmypaint get_color).
void paint_sample_box(const PaintTiles* layer, float x, float y, float radius, PaintRect box, float out[4]);

// a dab's colour after smudge pickup and mixing and the colour shifts; returns the target
// alpha (libmypaint eraser_target_alpha). Updates the dab's smudge bucket in place.
float paint_resolve_color(const PaintTiles* target, const PaintDab* dab, float* buckets, float color[3]);
