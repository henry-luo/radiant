// import of straight-alpha samples into rgba tiles, and presentation to sRGB u8 (PE7).
#include "paint_internal.hpp"
#include "lib/color.h"

struct PaintSrgbTables {
    float decode[256];           // u8 sRGB sample -> linear light
    float threshold[255];        // linear value where the encoded sample rounds up from k to k + 1
    PaintSrgbTables() {
        for (int k = 0; k < 256; k++) decode[k] = (float)color_srgb_to_linear(k / 255.0);
        for (int k = 0; k < 255; k++) threshold[k] = (float)color_srgb_to_linear((k + 0.5) / 255.0);
    }
};

static const PaintSrgbTables& paint_srgb_tables() {
    static const PaintSrgbTables tables;   // thread-safe one-time initialisation
    return tables;
}

float paint_srgb_decode_u8(uint8_t sample) { return paint_srgb_tables().decode[sample]; }

// counts the rounding thresholds at or below the value: the same sample that
// lround(encode(linear) * 255) yields, without a pow per channel.
uint8_t paint_srgb_encode_u8(float linear) {
    const float* t = paint_srgb_tables().threshold;
    if (!(linear >= t[0])) return 0;
    int low = 0, high = 255;     // invariant: t[low] <= linear, and t[high] > linear when high < 255
    while (high - low > 1) {
        int mid = (low + high) / 2;
        if (t[mid] <= linear) low = mid; else high = mid;
    }
    return (uint8_t)high;        // INT_CAST_OK: sample index in [1, 255]
}

static uint8_t paint_unit_u8(float value) {
    if (!(value > 0.0f)) return 0;
    if (value >= 1.0f) return 255;
    return (uint8_t)lroundf(value * 255.0f);  // INT_CAST_OK: u8 sample quantization
}

PaintResult paint_import(PaintTiles* target, const PaintView* source, int32_t origin_x, int32_t origin_y, bool decode_srgb) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(target, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (target->format != PAINT_FORMAT_RGBA) return paint_fail(PAINT_ERROR_ARGUMENT, "import needs an rgba layer");
    if (!source || !source->data || source->width <= 0 || source->height <= 0 ||
        (source->channels != 3 && source->channels != 4) || source->pixel_stride < source->channels ||
        source->row_stride < (int64_t)source->width * source->pixel_stride ||
        (source->elem != PAINT_ELEM_U8 && source->elem != PAINT_ELEM_F32))
        return paint_fail(PAINT_ERROR_ARGUMENT, "import needs u8 or f32 samples with 3 or 4 channels");
    PaintRect rect = {origin_x, origin_y, origin_x + source->width, origin_y + source->height};
    if (rect.x0 < 0) rect.x0 = 0; if (rect.y0 < 0) rect.y0 = 0;
    if (rect.x1 > target->width) rect.x1 = target->width; if (rect.y1 > target->height) rect.y1 = target->height;
    PaintRect none = {0, 0, 0, 0};
    if (paint_rect_empty(rect)) return paint_ok(none, 0);
    int32_t size = target->tile_size;
    for (int32_t ty = rect.y0 / size; ty <= (rect.y1 - 1) / size; ty++)
        for (int32_t tx = rect.x0 / size; tx <= (rect.x1 - 1) / size; tx++)
            if (!paint_tile(target, tx, ty)->data) return paint_fail(PAINT_ERROR_ARGUMENT, "a tile the import writes was not supplied");
    for (int32_t y = rect.y0; y < rect.y1; y++) for (int32_t x = rect.x0; x < rect.x1; x++) {
        int64_t at = (int64_t)(y - origin_y) * source->row_stride + (int64_t)(x - origin_x) * source->pixel_stride;
        float s[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        for (int32_t c = 0; c < source->channels; c++) {
            bool alpha = c == 3;
            if (source->elem == PAINT_ELEM_U8) {
                uint8_t v = ((const uint8_t*)source->data)[at + c];
                s[c] = decode_srgb && !alpha ? paint_srgb_decode_u8(v) : v / 255.0f;
            } else {
                float v = ((const float*)source->data)[at + c];
                if (!isfinite(v)) return paint_fail(PAINT_ERROR_ARGUMENT, "imported samples must be finite");
                v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
                s[c] = decode_srgb && !alpha ? (float)color_srgb_to_linear(v) : v;
            }
        }
        PaintView* view = paint_tile(target, x / size, y / size);
        float* d = (float*)view->data + (int64_t)(y % size) * view->row_stride + (int64_t)(x % size) * view->pixel_stride;
        d[0] = s[0] * s[3]; d[1] = s[1] * s[3]; d[2] = s[2] * s[3]; d[3] = s[3];
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (uint64_t)(rect.y1 - rect.y0));
}

PaintResult paint_present(const PaintTiles* layer, PaintRect rect, PaintView* out, const PaintPresentParams* params) {
    const char* reason = nullptr;
    if (!paint_tiles_valid(layer, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason);
    if (layer->format != PAINT_FORMAT_RGBA && layer->format != PAINT_FORMAT_PIGMENT)
        return paint_fail(PAINT_ERROR_ARGUMENT, "present needs an rgba or pigment layer");
    if (!params) return paint_fail(PAINT_ERROR_ARGUMENT, "present parameters are missing");
    if (paint_rect_empty(rect) || rect.x0 < 0 || rect.y0 < 0 || rect.x1 > layer->width || rect.y1 > layer->height)
        return paint_fail(PAINT_ERROR_ARGUMENT, "present rectangle must lie inside the layer");
    if (!out || !out->data || out->elem != PAINT_ELEM_U8 || out->channels != 4 ||
        out->width != rect.x1 - rect.x0 || out->height != rect.y1 - rect.y0 ||
        out->pixel_stride < 4 || out->row_stride < (int64_t)out->width * out->pixel_stride)
        return paint_fail(PAINT_ERROR_ARGUMENT, "present output must be rect-sized u8 RGBA");
    if (layer->format == PAINT_FORMAT_PIGMENT) {
        // a pigment layer shows its own substrate, so it takes no background.
        if (params->has_background) return paint_fail(PAINT_ERROR_ARGUMENT, "a pigment layer takes no present background");
        if (!params->band_matrix && layer->bands != 3) return paint_fail(PAINT_ERROR_ARGUMENT, "a pigment layer without 3 bands needs a band matrix");
        if (params->band_matrix)
            for (int32_t i = 0; i < layer->bands * 3; i++)
                if (!(params->band_matrix[i] >= -4.0f && params->band_matrix[i] <= 4.0f))
                    return paint_fail(PAINT_ERROR_ARGUMENT, "band matrix entries must be in [-4, 4]");
        if (params->has_lighting) {
            bool finite = isfinite(params->light[0]) && isfinite(params->light[1]) && isfinite(params->light[2]);
            if (!finite || !(params->light[2] > 0.0f)) return paint_fail(PAINT_ERROR_ARGUMENT, "light direction must be finite with z > 0");
            if (!(params->relief >= 0.0f && params->relief <= 1000.0f) || !(params->diffuse >= 0.0f && params->diffuse <= 1.0f) ||
                !(params->specular >= 0.0f && params->specular <= 1.0f) || !(params->shininess >= 1.0f && params->shininess <= 512.0f))
                return paint_fail(PAINT_ERROR_ARGUMENT, "lighting needs relief in [0, 1000], diffuse and specular in [0, 1], shininess in [1, 512]");
        }
        return paint_present_pigment(layer, rect, out, params);
    }
    if (params->has_lighting) return paint_fail(PAINT_ERROR_ARGUMENT, "lighting needs a pigment layer");
    float bg[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    if (params->has_background) {
        for (int c = 0; c < 4; c++)
            if (!(params->background[c] >= 0.0f && params->background[c] <= 1.0f))
                return paint_fail(PAINT_ERROR_ARGUMENT, "present background must be in [0, 1]");
        bg[3] = params->background[3];
        for (int c = 0; c < 3; c++) bg[c] = params->background[c] * bg[3];   // premultiply once
    }
    int32_t size = layer->tile_size;
    for (int32_t y = rect.y0; y < rect.y1; y++) for (int32_t x = rect.x0; x < rect.x1; x++) {
        const PaintView* view = paint_tile(layer, x / size, y / size);
        float p[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        if (view->data) {
            const float* s = (const float*)view->data + (int64_t)(y % size) * view->row_stride + (int64_t)(x % size) * view->pixel_stride;
            p[0] = s[0]; p[1] = s[1]; p[2] = s[2]; p[3] = s[3];
        }
        if (params->has_background) {
            float keep = 1.0f - p[3];
            for (int c = 0; c < 4; c++) p[c] += keep * bg[c];
        }
        uint8_t* d = (uint8_t*)out->data + (int64_t)(y - rect.y0) * out->row_stride + (int64_t)(x - rect.x0) * out->pixel_stride;
        if (p[3] <= 0.0f) { d[0] = d[1] = d[2] = d[3] = 0; continue; }
        float alpha = p[3] > 1.0f ? 1.0f : p[3];
        for (int c = 0; c < 3; c++) {
            float v = p[c] / p[3];
            d[c] = params->encode_srgb ? paint_srgb_encode_u8(v) : paint_unit_u8(v);
        }
        d[3] = paint_unit_u8(alpha);
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (uint64_t)(rect.y1 - rect.y0));
}
