#pragma once
// Shared wet sheet validation and deposition for dab and bristle targets (PE9).
#include "paint_wet.hpp"
#include "paint_internal.hpp"

template<typename T> static inline bool paint_wet_sheet_valid(const PaintWetSheetT<T>* s) {
    return s && s->cells && s->width >= 8 && s->height >= 8 && (int64_t)s->width * s->height <= INT32_MAX &&
        paint_wet_layout(s->pigments) && s->active.x0 >= 0 && s->active.y0 >= 0 &&
        s->active.x1 <= s->width && s->active.y1 <= s->height && s->active.x1 >= s->active.x0 &&
        s->active.y1 >= s->active.y0 && s->since_shrink >= 0 && s->wet_cells >= 0 &&
        s->wet_cells <= (int64_t)s->width * s->height;
}

template<typename T> static inline void paint_wet_expand(PaintWetSheetT<T>* s, PaintRect dirty) {
    if (paint_rect_empty(dirty)) return;
    // Use the reference watercolor activity margin around changed cells (PE9).
    PaintRect padded = {dirty.x0 > 24 ? dirty.x0-24 : 0, dirty.y0 > 24 ? dirty.y0-24 : 0,
        dirty.x1 < s->width-24 ? dirty.x1+24 : s->width, dirty.y1 < s->height-24 ? dirty.y1+24 : s->height};
    s->active = paint_rect_union(s->active, padded);
}

template<typename T> static inline void paint_wet_pixel(PaintWetSheetT<T>* s, int32_t channel, int64_t at,
        float mask, float coverage, float amount, float water) {
    int64_t n = (int64_t)s->width * s->height;
    T* wet = s->cells + n; T* pressure = s->cells + 3*n; T* pigment = s->cells + (8 + channel)*n;
    pigment[at] = fmin((T)1, pigment[at] + (T)(coverage * mask * amount));
    wet[at] = fmin((T)1, wet[at] + (T)(coverage * water));
    // The reference pigment stamp injects 0.18 pressure for 0.45 water (PE9).
    pressure[at] += (T)(coverage * mask * water * 0.4f);
}
