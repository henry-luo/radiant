// Wet dab deposition shares PE4's validated geometry and mask (PE9).
#include "paint_wet_internal.hpp"
#include <string.h>

// Validate the wet fields separately, then decode the neutralized common row through PE4.
static bool wet_decode(const float* row, int32_t w, int32_t h, int32_t pigments,
        const PaintDabParams* params, PaintDab* dab, const char** reason) {
    float pigment = row[PAINT_DAB_PIGMENT], amount = row[PAINT_DAB_AMOUNT], water = row[PAINT_DAB_WATER];
    if (!isfinite(pigment) || pigment < 0 || pigment >= pigments || floorf(pigment) != pigment ||
        !isfinite(amount) || amount < 0 || amount > 64 || !isfinite(water) || water < 0 || water > 64) {
        *reason = "wet dab needs a valid pigment and finite amount and water in [0, 64]"; return false;
    }
    bool shifted = false;
    for (int i = PAINT_DAB_DH; i <= PAINT_DAB_DHSV_S; ++i) shifted |= row[i] != 0;
    if (row[PAINT_DAB_THICKNESS] != 0 || row[PAINT_DAB_ERASER] != 1 || row[PAINT_DAB_LOCK_ALPHA] != 0 ||
        row[PAINT_DAB_COLORIZE] != 0 || row[PAINT_DAB_POSTERIZE] != 0 || row[PAINT_DAB_BLEND] != 0 ||
        row[PAINT_DAB_SMUDGE] != 0 || row[PAINT_DAB_SMUDGE_RADIUS] != 0 || shifted) {
        *reason = "wet dab does not support erasing, smudge, body or recolouring"; return false;
    }
    float neutral[PAINT_DAB_COLUMNS]; memcpy(neutral, row, sizeof(neutral));
    neutral[PAINT_DAB_PIGMENT] = -1; neutral[PAINT_DAB_AMOUNT] = 0; neutral[PAINT_DAB_WATER] = 0;
    return paint_dab_decode(neutral, w, h, params, dab, reason);
}

template<typename T> static PaintResult wet_dabs(PaintWetSheetT<T>* sheet, const float* rows, int64_t count,
        int32_t columns, const PaintDabParams* params, bool apply) {
    if (!paint_wet_sheet_valid(sheet) || !params || columns != PAINT_DAB_COLUMNS || count < 0 || (count && !rows))
        return paint_fail(PAINT_ERROR_ARGUMENT, "invalid wet sheet or dab table");
    // Validate shared planes and parameters even for an empty batch.
    PaintResult check = paint_dab_bounds(nullptr, 0, columns, sheet->width, sheet->height, 1, params, nullptr);
    if (check.status != PAINT_OK) return check;
    const char* reason = nullptr; PaintRect dirty = {0, 0, 0, 0}; uint64_t work = 0;
    for (int64_t i = 0; i < count; ++i) {
        PaintDab dab;
        if (!wet_decode(rows + i * columns, sheet->width, sheet->height, sheet->pigments, params, &dab, &reason))
            return paint_fail(PAINT_ERROR_ARGUMENT, reason);
        uint64_t area = paint_rect_empty(dab.box) ? 0 : (uint64_t)(dab.box.x1-dab.box.x0)*(dab.box.y1-dab.box.y0);
        if (area > params->work_budget - work) return paint_fail(PAINT_ERROR_BUDGET, "wet dabs exceed the bounded work budget");
        work += area; dirty = paint_rect_union(dirty, dab.box);
    }
    if (!apply) return paint_ok(dirty, work);
    for (int64_t i = 0; i < count; ++i) {
        const float* row = rows + i * columns; PaintDab dab;
        wet_decode(row, sheet->width, sheet->height, sheet->pigments, params, &dab, &reason);
        for (int32_t y = dab.box.y0; y < dab.box.y1; ++y) for (int32_t x = dab.box.x0; x < dab.box.x1; ++x) {
            float m = paint_dab_mask(&dab, x + 0.5f - dab.x, y + 0.5f - dab.y);
            m = paint_grained(&dab, params->paper, m, x, y);
            int64_t at = (int64_t)y * sheet->width + x;
            paint_wet_pixel(sheet, (int32_t)row[PAINT_DAB_PIGMENT], at, m, m * dab.opacity * dab.flow,
                row[PAINT_DAB_AMOUNT], row[PAINT_DAB_WATER]);
        }
    }
    paint_wet_expand(sheet, dirty);
    return paint_ok(dirty, work);
}

PaintResult paint_wet_dabs_bounds(PaintWetSheet* s, const float* rows, int64_t n, int32_t cols, const PaintDabParams* p) {
    return wet_dabs(s, rows, n, cols, p, false);
}
PaintResult paint_wet_dabs(PaintWetSheet* s, const float* rows, int64_t n, int32_t cols, const PaintDabParams* p) {
    return wet_dabs(s, rows, n, cols, p, true);
}
PaintResult paint_wet_dabs_bounds64(PaintWetSheet64* s, const float* rows, int64_t n, int32_t cols, const PaintDabParams* p) {
    return wet_dabs(s, rows, n, cols, p, false);
}
PaintResult paint_wet_dabs64(PaintWetSheet64* s, const float* rows, int64_t n, int32_t cols, const PaintDabParams* p) {
    return wet_dabs(s, rows, n, cols, p, true);
}
