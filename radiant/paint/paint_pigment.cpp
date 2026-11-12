// the pigment medium (PE8): a wet Kubelka-Munk film over a substrate. Deposit is additive in the
// thickness-weighted quantities tK, tS and t, so dabs of different pigments mix by concentration
// and commute; nothing divides until display. Display composes the film over the substrate with
// Kubelka's layer formula, and drying folds the film into the substrate the same way.
#include "paint_wet_internal.hpp"

// the mask-weighted mean film under a dab's pickup footprint, with libmypaint get_color's round
// hardness-0.5 footprint (PE5); clean tiles count as no film.
static void paint_film_sample(const PaintTiles* layer, const PaintDab* dab, float* out) {
    int32_t fields = paint_film_fields(layer->bands), size = layer->tile_size;
    PaintDab foot = {};
    float radius = dab->radius * dab->smudge_radius;
    foot.x = dab->x; foot.y = dab->y; foot.radius = radius < 0.5f ? 0.5f : radius; foot.aspect = 1.0f;
    foot.cos_angle = 1.0f; foot.hardness = 0.5f; foot.profile = PAINT_PROFILE_MYPAINT;
    double sum[2 * PAINT_MAX_BANDS + 1] = {0}, weight = 0;
    for (int32_t py = dab->pickup.y0; py < dab->pickup.y1; py++) for (int32_t px = dab->pickup.x0; px < dab->pickup.x1; px++) {
        float m = paint_dab_mask(&foot, (float)px + 0.5f - dab->x, (float)py + 0.5f - dab->y);
        if (m <= 0.0f) continue;
        weight += m;
        const PaintView* view = paint_tile(layer, px / size, py / size);
        if (!view->data) continue;
        const float* p = (const float*)view->data + (int64_t)(py % size) * view->row_stride + (int64_t)(px % size) * view->pixel_stride;
        for (int32_t f = 0; f < fields; f++) sum[f] += m * p[f];
    }
    for (int32_t f = 0; f < fields; f++) out[f] = weight > 0 ? (float)(sum[f] / weight) : 0.0f;
}

void paint_pigment_dab(PaintTiles* target, const PaintDab* dab, const PaintDabParams* params) {
    int32_t bands = target->bands, fields = paint_film_fields(bands), size = target->tile_size;
    float* bucket = params->smudge && params->smudge_buckets > 0
        ? params->smudge + (int64_t)dab->smudge_bucket * fields : nullptr;
    // the bucket follows the film under the dab; smudge_length keeps part of what it carried.
    if (!paint_rect_empty(dab->pickup) && bucket) {
        float picked[2 * PAINT_MAX_BANDS + 1];
        paint_film_sample(target, dab, picked);
        float keep = fmaxf(0.01f, dab->smudge_length);
        for (int32_t f = 0; f < fields; f++) bucket[f] = keep * bucket[f] + (1.0f - keep) * picked[f];
    }
    if (paint_rect_empty(dab->box)) return;
    // the brush's paint per unit coverage: amount scales the pigment, thickness the paint body.
    float ink[2 * PAINT_MAX_BANDS + 1];
    const float* pigment = params->pigments + (int64_t)dab->pigment * 2 * bands;
    for (int32_t b = 0; b < 2 * bands; b++) ink[b] = dab->amount * pigment[b];
    ink[2 * bands] = dab->thickness;
    float smudge = bucket ? dab->smudge : 0.0f;
    for (int32_t ty = dab->box.y0 / size; ty <= (dab->box.y1 - 1) / size; ty++) {
        int32_t y0 = ty * size > dab->box.y0 ? ty * size : dab->box.y0;
        int32_t y1 = (ty + 1) * size < dab->box.y1 ? (ty + 1) * size : dab->box.y1;
        for (int32_t tx = dab->box.x0 / size; tx <= (dab->box.x1 - 1) / size; tx++) {
            int32_t x0 = tx * size > dab->box.x0 ? tx * size : dab->box.x0;
            int32_t x1 = (tx + 1) * size < dab->box.x1 ? (tx + 1) * size : dab->box.x1;
            PaintView* view = paint_tile(target, tx, ty);
            for (int32_t y = y0; y < y1; y++) {
                float dy = (float)y + 0.5f - dab->y;
                float* row = (float*)view->data + (int64_t)(y - ty * size) * view->row_stride;
                for (int32_t x = x0; x < x1; x++) {
                    float m = paint_dab_mask(dab, (float)x + 0.5f - dab->x, dy);
                    if (m <= 0.0f) continue;
                    float c = paint_grained(dab, params->paper, m, x, y) * dab->opacity * dab->flow;
                    if (c <= 0.0f) continue;
                    if (c > 1.0f) c = 1.0f;
                    paint_film_apply(row + (int64_t)(x - tx * size) * view->pixel_stride, fields, c, dab->eraser,
                        smudge, bucket, ink);
                }
            }
        }
    }
}

// Kubelka's reflectance of a film over a substrate of reflectance rg (PE8). With K = tK / t,
// S = tS / t and thickness t, the formula reads only K/S = tK / tS and S t = tS, so the film's
// thickness cancels and no division by t happens. sinh and cosh share one exponential.
static double paint_km(double tk, double ts, double rg) {
    if (tk <= 0.0 && ts <= 0.0) return rg;
    if (ts <= 1.0e-9) return rg * exp(-2.0 * tk);   // a pure absorber: light crosses it twice
    double a = 1.0 + tk / ts, b = sqrt(a * a - 1.0), x = b * ts;
    if (x > 40.0) return 1.0 / (a + b);              // opaque: R_inf = a - b, in its stable form
    double e = exp(x), ch = 0.5 * (e + 1.0 / e);
    double sh_b = b > 1.0e-6 ? 0.5 * (e - 1.0 / e) / b : ts * (1.0 + x * x / 6.0);
    return (sh_b * (1.0 - rg * a) + rg * ch) / (sh_b * (a - rg) + ch);
}

// one pigment pixel's fields, or null for a clean tile (no film, the layer's substrate, no relief).
static inline const float* paint_pigment_at(const PaintTiles* layer, int32_t x, int32_t y) {
    const PaintView* view = paint_tile(layer, x / layer->tile_size, y / layer->tile_size);
    if (!view->data) return nullptr;
    return (const float*)view->data + (int64_t)(y % layer->tile_size) * view->row_stride +
        (int64_t)(x % layer->tile_size) * view->pixel_stride;
}

// the lighting height at a pixel clamped into the layer: dried relief plus wet film thickness.
static inline float paint_pigment_height(const PaintTiles* layer, int32_t x, int32_t y) {
    x = x < 0 ? 0 : (x >= layer->width ? layer->width - 1 : x);
    y = y < 0 ? 0 : (y >= layer->height ? layer->height - 1 : y);
    const float* p = paint_pigment_at(layer, x, y);
    return p ? p[paint_pigment_h(layer->bands)] + p[paint_pigment_t(layer->bands)] : 0.0f;
}

// The optional wet sheet contributes to the same thickness-weighted film as PE8 deposits.
static PaintResult paint_pigment_render(const PaintTiles* layer, PaintRect rect, PaintView* out, const PaintPresentParams* params,
        const void* cells, bool wide, int32_t pigments, const float* table) {
    int32_t bands = layer->bands;
    float light[3] = {0.0f, 0.0f, 1.0f}, half[3] = {0.0f, 0.0f, 1.0f};
    if (params->has_lighting) {
        float n = sqrtf(params->light[0] * params->light[0] + params->light[1] * params->light[1] + params->light[2] * params->light[2]);
        for (int i = 0; i < 3; i++) light[i] = params->light[i] / n;
        float hn = sqrtf(light[0] * light[0] + light[1] * light[1] + (light[2] + 1.0f) * (light[2] + 1.0f));
        half[0] = light[0] / hn; half[1] = light[1] / hn; half[2] = (light[2] + 1.0f) / hn;
    }
    // a flat film keeps its colour: the shade is relative to a flat normal, and so is the highlight.
    float flat_highlight = params->has_lighting ? powf(half[2], params->shininess) : 0.0f;
    for (int32_t y = rect.y0; y < rect.y1; y++) for (int32_t x = rect.x0; x < rect.x1; x++) {
        const float* p = paint_pigment_at(layer, x, y);
        double reflect[PAINT_MAX_BANDS];
        for (int32_t b = 0; b < bands; b++) {
            double k = p ? p[b] : 0.0, s = p ? p[bands+b] : 0.0;
            if (cells) {
                int64_t n = (int64_t)layer->width * layer->height, at = (int64_t)y * layer->width + x;
                for (int32_t j = 0; j < pigments; ++j) {
                    int64_t g = (int64_t)(8+j)*n+at, d = (int64_t)(8+pigments+j)*n+at;
                    double amount = wide ? ((const double*)cells)[g] + ((const double*)cells)[d]
                                         : (double)((const float*)cells)[g] + ((const float*)cells)[d];
                    k += amount * table[j*2*bands+b]; s += amount * table[j*2*bands+bands+b];
                }
            }
            reflect[b] = paint_km(k, s, p ? p[paint_pigment_r(bands)+b] : layer->substrate[b]);
        }
        float rgb[3];
        for (int c = 0; c < 3; c++) {
            double v = 0;
            if (params->band_matrix) for (int32_t b = 0; b < bands; b++) v += reflect[b] * params->band_matrix[b * 3 + c];
            else v = reflect[c];
            rgb[c] = (float)v;
        }
        if (params->has_lighting) {
            float dx = 0.5f * params->relief * (paint_pigment_height(layer, x + 1, y) - paint_pigment_height(layer, x - 1, y));
            float dy = 0.5f * params->relief * (paint_pigment_height(layer, x, y + 1) - paint_pigment_height(layer, x, y - 1));
            float nl = 1.0f / sqrtf(dx * dx + dy * dy + 1.0f), normal[3] = {-dx * nl, -dy * nl, nl};
            float lit = normal[0] * light[0] + normal[1] * light[1] + normal[2] * light[2];
            float shade = 1.0f + params->diffuse * (fmaxf(lit, 0.0f) / light[2] - 1.0f);
            float facing = fmaxf(0.0f, normal[0] * half[0] + normal[1] * half[1] + normal[2] * half[2]);
            float highlight = params->specular * fmaxf(0.0f, powf(facing, params->shininess) - flat_highlight);
            for (int c = 0; c < 3; c++) rgb[c] = rgb[c] * fmaxf(shade, 0.0f) + highlight;
        }
        uint8_t* d = (uint8_t*)out->data + (int64_t)(y - rect.y0) * out->row_stride + (int64_t)(x - rect.x0) * out->pixel_stride;
        for (int c = 0; c < 3; c++) {
            float v = paint_clamp01(rgb[c]);
            d[c] = params->encode_srgb ? paint_srgb_encode_u8(v) : (uint8_t)lroundf(v * 255.0f);  // INT_CAST_OK: u8 sample quantization
        }
        d[3] = 255;
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (uint64_t)(rect.y1 - rect.y0));
}

PaintResult paint_present_pigment(const PaintTiles* layer, PaintRect rect, PaintView* out, const PaintPresentParams* params) {
    return paint_pigment_render(layer, rect, out, params, nullptr, false, 0, nullptr);
}

template<typename T> static PaintResult paint_wet_present_impl(const PaintWetSheetT<T>* sheet, const PaintTiles* layer,
        PaintRect rect, PaintView* out, const float* table, int32_t count, bool encode, uint64_t budget) {
    const char* reason = nullptr;
    if (!paint_wet_sheet_valid(sheet) || !paint_tiles_valid(layer, &reason))
        return paint_fail(PAINT_ERROR_ARGUMENT, reason ? reason : "invalid wet sheet");
    if (layer->format != PAINT_FORMAT_PIGMENT || layer->bands != 3 || layer->width != sheet->width ||
        layer->height != sheet->height || count != sheet->pigments || !table || paint_rect_empty(rect) ||
        rect.x0 < 0 || rect.y0 < 0 || rect.x1 > sheet->width || rect.y1 > sheet->height)
        return paint_fail(PAINT_ERROR_ARGUMENT, "wet present needs a matching three-band layer, table and rectangle");
    if (!out || !out->data || out->elem != PAINT_ELEM_U8 || out->channels != 4 ||
        out->width != rect.x1-rect.x0 || out->height != rect.y1-rect.y0 || out->pixel_stride < 4 ||
        out->row_stride < (int64_t)out->width*out->pixel_stride)
        return paint_fail(PAINT_ERROR_ARGUMENT, "wet present output must be rect-sized u8 RGBA");
    uint64_t area = (uint64_t)(rect.x1-rect.x0)*(rect.y1-rect.y0);
    if (area > budget / (uint64_t)count) return paint_fail(PAINT_ERROR_BUDGET, "wet present exceeds the bounded work budget");
    for (int32_t i=0; i<count*6; ++i)
        if (!isfinite(table[i]) || table[i] < 0 || table[i] > 10000)
            return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet pigment table");
    int64_t n = (int64_t)sheet->width*sheet->height;
    // Reject an invalid sheet before writing any output (D7.6.4).
    for (int32_t y=rect.y0; y<rect.y1; ++y) for (int32_t x=rect.x0; x<rect.x1; ++x) {
        int64_t at = (int64_t)y*sheet->width+x;
        for (int32_t j=0; j<count; ++j) {
            T g=sheet->cells[(int64_t)(8+j)*n+at], d=sheet->cells[(int64_t)(8+count+j)*n+at];
            if (!isfinite(g) || !isfinite(d) || g < 0 || d < 0 || g > 1 || d > 1)
                return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet pigment concentration");
        }
    }
    PaintPresentParams params = {encode, false, {0,0,0,0}, false, {0,0,1}, 0,0,0,1,nullptr};
    PaintResult result = paint_pigment_render(layer, rect, out, &params, sheet->cells, sizeof(T)==sizeof(double), count, table);
    result.work = area * (uint64_t)count;
    return result;
}

PaintResult paint_wet_present(const PaintWetSheet* s, const PaintTiles* l, PaintRect r, PaintView* o,
        const float* table, int32_t count, bool encode, uint64_t budget) {
    return paint_wet_present_impl(s,l,r,o,table,count,encode,budget);
}
PaintResult paint_wet_present64(const PaintWetSheet64* s, const PaintTiles* l, PaintRect r, PaintView* o,
        const float* table, int32_t count, bool encode, uint64_t budget) {
    return paint_wet_present_impl(s,l,r,o,table,count,encode,budget);
}

PaintResult paint_pigment_dry(PaintTiles* layer, PaintRect rect, float rate) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(layer, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (layer->format != PAINT_FORMAT_PIGMENT) return paint_fail(PAINT_ERROR_ARGUMENT, "drying needs a pigment layer");
    if (!(rate > 0.0f && rate <= 1.0f)) return paint_fail(PAINT_ERROR_ARGUMENT, "drying rate must be in (0, 1]");
    if (rect.x0 < 0) rect.x0 = 0; if (rect.y0 < 0) rect.y0 = 0;
    if (rect.x1 > layer->width) rect.x1 = layer->width; if (rect.y1 > layer->height) rect.y1 = layer->height;
    PaintRect none = {0, 0, 0, 0};
    if (paint_rect_empty(rect)) return paint_ok(none, 0);
    int32_t bands = layer->bands, fields = paint_film_fields(bands), size = layer->tile_size;
    for (int32_t ty = rect.y0 / size; ty <= (rect.y1 - 1) / size; ty++) {
        int32_t y0 = ty * size > rect.y0 ? ty * size : rect.y0, y1 = (ty + 1) * size < rect.y1 ? (ty + 1) * size : rect.y1;
        for (int32_t tx = rect.x0 / size; tx <= (rect.x1 - 1) / size; tx++) {
            PaintView* view = paint_tile(layer, tx, ty);
            if (!view->data) continue;   // a clean tile has no film to dry
            int32_t x0 = tx * size > rect.x0 ? tx * size : rect.x0, x1 = (tx + 1) * size < rect.x1 ? (tx + 1) * size : rect.x1;
            for (int32_t y = y0; y < y1; y++) for (int32_t x = x0; x < x1; x++) {
                float* p = (float*)view->data + (int64_t)(y - ty * size) * view->row_stride + (int64_t)(x - tx * size) * view->pixel_stride;
                float t = p[paint_pigment_t(bands)];
                // KM layers of one material compose: rate of the film over the substrate, then the rest over that.
                for (int32_t b = 0; b < bands; b++) {
                    float* r = p + paint_pigment_r(bands) + b;
                    *r = paint_clamp01((float)paint_km(rate * p[b], rate * p[bands + b], *r));
                }
                for (int32_t f = 0; f < fields; f++) p[f] *= 1.0f - rate;
                p[paint_pigment_h(bands)] += rate * t;
            }
        }
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (uint64_t)(rect.y1 - rect.y0));
}
