// colour sampling, smudge buckets and colour shifts (PE4, PE5), after libmypaint's model:
// get_color averages under a round footprint; smudge buckets hold premultiplied colour.
#include "paint_internal.hpp"

void paint_sample_box(const PaintTiles* layer, float x, float y, float radius, PaintRect box, float out[4]) {
    // the footprint libmypaint's get_color uses: a round dab with hardness 0.5.
    PaintDab foot = {};
    foot.x = x; foot.y = y; foot.radius = radius < 0.5f ? 0.5f : radius; foot.aspect = 1.0f;
    foot.cos_angle = 1.0f; foot.hardness = 0.5f; foot.profile = PAINT_PROFILE_MYPAINT;
    double sum[4] = {0, 0, 0, 0}, weight = 0;
    int32_t size = layer->tile_size;
    for (int32_t py = box.y0; py < box.y1; py++) for (int32_t px = box.x0; px < box.x1; px++) {
        float m = paint_dab_mask(&foot, (float)px + 0.5f - x, (float)py + 0.5f - y);
        if (m <= 0.0f) continue;
        weight += m;
        const PaintView* view = paint_tile(layer, px / size, py / size);
        if (!view->data) continue;
        const float* p = (const float*)view->data + (int64_t)(py % size) * view->row_stride + (int64_t)(px % size) * view->pixel_stride;
        for (int c = 0; c < 4; c++) sum[c] += m * p[c];
    }
    if (weight <= 0 || sum[3] <= 0) { out[0] = out[1] = out[2] = out[3] = 0.0f; return; }
    out[3] = paint_clamp01((float)(sum[3] / weight));
    for (int c = 0; c < 3; c++) out[c] = paint_clamp01((float)(sum[c] / sum[3]));   // straight colour
}

PaintResult paint_sample(const PaintTiles* layer, float x, float y, float radius, float out[4]) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(layer, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (layer->format != PAINT_FORMAT_RGBA) return paint_fail(PAINT_ERROR_ARGUMENT, "sampling needs an rgba layer");
    if (!isfinite(x) || !isfinite(y) || !isfinite(radius) || radius < 0.0f || radius > 8192.0f)
        return paint_fail(PAINT_ERROR_ARGUMENT, "sample position must be finite and radius in [0, 8192]");
    double extent = radius < 0.5f ? 0.5 : (double)radius;
    double left = floor(x - extent), top = floor(y - extent), right = ceil(x + extent), bottom = ceil(y + extent);
    PaintRect box = {(int32_t)fmax(0.0, left), (int32_t)fmax(0.0, top),  // INT_CAST_OK: clipped pixel bounds
                     (int32_t)fmin((double)layer->width, right), (int32_t)fmin((double)layer->height, bottom)};
    if (paint_rect_empty(box)) { out[0] = out[1] = out[2] = out[3] = 0.0f; return paint_ok(box, 0); }
    paint_sample_box(layer, x, y, radius, box, out);
    return paint_ok(box, (uint64_t)(box.x1 - box.x0) * (uint64_t)(box.y1 - box.y0));
}

static void paint_rgb_to_hsv(const float* rgb, float* h, float* s, float* v) {
    float mx = fmaxf(rgb[0], fmaxf(rgb[1], rgb[2])), mn = fminf(rgb[0], fminf(rgb[1], rgb[2])), d = mx - mn;
    *v = mx; *s = mx > 0.0f ? d / mx : 0.0f; *h = 0.0f;
    if (d <= 0.0f) return;
    if (mx == rgb[0]) *h = (rgb[1] - rgb[2]) / d;
    else if (mx == rgb[1]) *h = 2.0f + (rgb[2] - rgb[0]) / d;
    else *h = 4.0f + (rgb[0] - rgb[1]) / d;
    *h /= 6.0f;
    if (*h < 0.0f) *h += 1.0f;
}

static void paint_hsv_to_rgb(float h, float s, float v, float* rgb) {
    h = (h - floorf(h)) * 6.0f;
    int32_t sector = (int32_t)h;   // INT_CAST_OK: hue sector 0..5
    float f = h - (float)sector, p = v * (1.0f - s), q = v * (1.0f - s * f), t = v * (1.0f - s * (1.0f - f));
    switch (sector % 6) {
    case 0: rgb[0] = v; rgb[1] = t; rgb[2] = p; break;
    case 1: rgb[0] = q; rgb[1] = v; rgb[2] = p; break;
    case 2: rgb[0] = p; rgb[1] = v; rgb[2] = t; break;
    case 3: rgb[0] = p; rgb[1] = q; rgb[2] = v; break;
    case 4: rgb[0] = t; rgb[1] = p; rgb[2] = v; break;
    default: rgb[0] = v; rgb[1] = p; rgb[2] = q; break;
    }
}

// HSL keeps the hue of HSV; lightness and saturation are its own.
static void paint_rgb_to_hsl(const float* rgb, float* h, float* s, float* l) {
    float mx = fmaxf(rgb[0], fmaxf(rgb[1], rgb[2])), mn = fminf(rgb[0], fminf(rgb[1], rgb[2])), v;
    paint_rgb_to_hsv(rgb, h, s, &v);
    *l = 0.5f * (mx + mn);
    float d = mx - mn;
    *s = d <= 0.0f ? 0.0f : d / (1.0f - fabsf(2.0f * *l - 1.0f));
}

static void paint_hsl_to_rgb(float h, float s, float l, float* rgb) {
    float c = (1.0f - fabsf(2.0f * l - 1.0f)) * s, v = l + 0.5f * c;
    paint_hsv_to_rgb(h, v > 0.0f ? c / v : 0.0f, v, rgb);
}

// libmypaint change_color_*: hue, HSV saturation and value first, then HSL lightness and saturation.
static void paint_shift_color(const float* shift, float* rgb) {
    if (shift[0] == 0.0f && shift[1] == 0.0f && shift[2] == 0.0f && shift[3] == 0.0f && shift[4] == 0.0f) return;
    float h, s, v;
    paint_rgb_to_hsv(rgb, &h, &s, &v);
    paint_hsv_to_rgb(h + shift[0], paint_clamp01(s + shift[4]), paint_clamp01(v + shift[3]), rgb);
    if (shift[1] == 0.0f && shift[2] == 0.0f) return;
    float l;
    paint_rgb_to_hsl(rgb, &h, &s, &l);
    paint_hsl_to_rgb(h, paint_clamp01(s + shift[2]), paint_clamp01(l + shift[1]), rgb);
}

float paint_resolve_color(const PaintTiles* target, const PaintDab* dab, float* buckets, float color[3]) {
    for (int c = 0; c < 3; c++) color[c] = dab->color[c];
    float target_alpha = 1.0f;
    float* bucket = buckets ? buckets + (int64_t)dab->smudge_bucket * PAINT_SMUDGE_FIELDS : nullptr;
    if (!paint_rect_empty(dab->pickup) && bucket) {
        // the bucket follows the canvas under the dab; smudge_length keeps part of the old colour.
        float picked[4];
        paint_sample_box(target, dab->x, dab->y, fmaxf(0.5f, dab->radius * dab->smudge_radius), dab->pickup, picked);
        float keep = fmaxf(0.01f, dab->smudge_length);
        bucket[3] = paint_clamp01(keep * bucket[3] + (1.0f - keep) * picked[3]);
        for (int c = 0; c < 3; c++) bucket[c] = keep * bucket[c] + (1.0f - keep) * picked[c] * picked[3];
    }
    if (dab->smudge > 0.0f && bucket) {
        // a transparent bucket makes the dab erase toward its transparency (libmypaint).
        float s = dab->smudge;
        target_alpha = paint_clamp01((1.0f - s) + s * bucket[3]);
        if (target_alpha > 0.0f)
            for (int c = 0; c < 3; c++) color[c] = paint_clamp01((s * bucket[c] + (1.0f - s) * dab->color[c]) / target_alpha);
    }
    paint_shift_color(dab->shift, color);
    return target_alpha * dab->eraser;
}
