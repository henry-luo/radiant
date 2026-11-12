// Transfer dry watercolor deposits to the pigment film (PE9, D5.5.2).
#include "paint_wet.hpp"
#include "paint_internal.hpp"

template<typename T, typename Sheet>
static PaintResult wet_deposit_preflight(Sheet* sheet, PaintTiles* layer, PaintRect rect,
        const float* pigments, int32_t pigment_count, uint8_t* touched, bool require_tiles) {
    const char* reason = nullptr;
    if (!sheet || !sheet->cells || sheet->width <= 0 || sheet->height <= 0 ||
        !paint_tiles_valid(layer, &reason)) return paint_fail(PAINT_ERROR_ARGUMENT, reason ? reason : "invalid wet sheet");
    if (layer->format != PAINT_FORMAT_PIGMENT || layer->width != sheet->width || layer->height != sheet->height ||
        !pigments || !paint_wet_layout(pigment_count) || pigment_count != sheet->pigments || rect.x0 < 0 || rect.y0 < 0 || rect.x1 < rect.x0 ||
        rect.y1 < rect.y0 || rect.x1 > sheet->width || rect.y1 > sheet->height)
        return paint_fail(PAINT_ERROR_ARGUMENT, "wet deposit needs a matching pigment layer, table and rectangle");
    // The caller owns the plane buffer; reject spans that cannot be indexed safely (PE11).
    if ((uint64_t)sheet->width * (uint64_t)sheet->height > INT32_MAX ||
        (uint64_t)sheet->width * (uint64_t)sheet->height >
            SIZE_MAX / (sizeof(T) * (size_t)paint_wet_layout(pigment_count)))
        return paint_fail(PAINT_ERROR_ARGUMENT, "wet sheet span is too large");
    int32_t bands = layer->bands, size = layer->tile_size;
    for (int k = 0; k < pigment_count * bands * 2; ++k)
        if (!isfinite(pigments[k]) || pigments[k] < 0.0f || pigments[k] > 10000.0f)
            return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet pigment table");
    int64_t n = (int64_t)sheet->width * sheet->height;
    T* wet = sheet->cells + n;
    // Preflight all inputs and tiles before changing either owner.
    for (int y = rect.y0; y < rect.y1; ++y) for (int x = rect.x0; x < rect.x1; ++x) {
        int64_t i = (int64_t)y * sheet->width + x;
        bool has_pigment = false;
        for (int k = 0; k < pigment_count; ++k) {
            T v = sheet->cells[(paint_wet_deposited(pigment_count) + k) * n + i];
            if (!isfinite(v) || v < 0.0f || v > 1.0f) return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet pigment deposit");
            has_pigment |= v > 0.0f;
        }
        if (has_pigment && (!isfinite(wet[i]) || wet[i] >= 0.025f || wet[i] < 0.0f))
            return paint_fail(PAINT_ERROR_ARGUMENT, "wet pigment must dry before transfer");
        if (has_pigment) {
            if (touched) touched[(y / size) * layer->tiles_x + x / size] = 1;
            if (require_tiles && !paint_tile(layer, x / size, y / size)->data)
                return paint_fail(PAINT_ERROR_ARGUMENT, "wet deposit needs allocated destination tiles");
        }
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (rect.y1 - rect.y0));
}



template<typename T, typename Sheet>
static PaintResult wet_deposit(Sheet* sheet, PaintTiles* layer, PaintRect rect,
        const float* pigments, int32_t pigment_count) {
    PaintResult valid = wet_deposit_preflight<T>(sheet, layer, rect, pigments, pigment_count, nullptr, true);
    if (valid.status != PAINT_OK) return valid;
    int32_t bands = layer->bands, size = layer->tile_size;
    int64_t n = (int64_t)sheet->width * sheet->height;
    for (int y = rect.y0; y < rect.y1; ++y) for (int x = rect.x0; x < rect.x1; ++x) {
        int64_t i = (int64_t)y * sheet->width + x;
        PaintView* tile = paint_tile(layer, x / size, y / size);
        if (!tile->data) continue;
        float* pixel = (float*)tile->data + (int64_t)(y % size) * tile->row_stride + (int64_t)(x % size) * tile->pixel_stride;
        for (int k = 0; k < pigment_count; ++k) {
            T* deposit = sheet->cells + (paint_wet_deposited(pigment_count) + k) * n + i;
            float amount = (float)*deposit;
            const float* row = pigments + k * 2 * bands;
            for (int b = 0; b < 2 * bands; ++b) pixel[b] += amount * row[b];
            pixel[paint_pigment_t(bands)] += amount;
            *deposit = 0.0f;
        }
    }
    return paint_ok(rect, (uint64_t)(rect.x1 - rect.x0) * (rect.y1 - rect.y0));
}

PaintResult paint_wet_deposit_bounds(PaintWetSheet* s, PaintTiles* l, PaintRect r, const float* p, int32_t n, uint8_t* t) { return wet_deposit_preflight<float>(s,l,r,p,n,t,false); }
PaintResult paint_wet_deposit(PaintWetSheet* s, PaintTiles* l, PaintRect r, const float* p, int32_t n) { return wet_deposit<float>(s,l,r,p,n); }
PaintResult paint_wet_deposit_bounds64(PaintWetSheet64* s, PaintTiles* l, PaintRect r, const float* p, int32_t n, uint8_t* t) { return wet_deposit_preflight<double>(s,l,r,p,n,t,false); }
PaintResult paint_wet_deposit64(PaintWetSheet64* s, PaintTiles* l, PaintRect r, const float* p, int32_t n) { return wet_deposit<double>(s,l,r,p,n); }
