#pragma once
// Caller-owned wet sheet and scratch planes (PE9, D5.5.2).
#include "paint.hpp"

// Eight fixed planes, three planes per pigment and four shared blur planes (PE9).
static const int32_t PAINT_WET_MAX_PIGMENTS = 4096;
static inline int32_t paint_wet_layout(int32_t pigments) {
    return pigments >= 1 && pigments <= PAINT_WET_MAX_PIGMENTS ? 12 + 3 * pigments : 0;
}
static inline int32_t paint_wet_deposited(int32_t pigments) { return 8 + pigments; }
static inline int32_t paint_wet_scratch(int32_t pigments) { return 8 + 2 * pigments; }
static const int32_t PAINT_WET_PLANES = 21; // three-pigment watercolor reference
template<typename T> struct PaintWetSheetT {
    T* cells;                // contiguous T[paint_wet_layout(pigments), height, width]
    int32_t width, height;
    PaintRect active;            // half-open; empty means no tracked activity
    int32_t since_shrink, wet_cells;
    int32_t pigments = 3;        // the reference uses three pigments
};
using PaintWetSheet = PaintWetSheetT<float>;
using PaintWetSheet64 = PaintWetSheetT<double>;
struct PaintWetParams {
    float wet_diffusion, pigment_diffusion;
    int32_t edge_kernel, edge_kernel_large;
    float velocity_clamp, tilt, evaporation;
    bool edges, drying;
    float adsorption[9];         // three-pigment inline table
    const float* adsorption_table = nullptr;  // optional full table, required for more than three
};
// Validates before writing. Scratch planes belong to the caller; no pointers are retained.
PaintResult paint_wet_step(PaintWetSheet* sheet, const PaintWetParams* params, int32_t steps);
PaintResult paint_wet_step64(PaintWetSheet64* sheet, const PaintWetParams* params, int32_t steps);
// Settle pigment in place without advancing the flow simulation (PE9).
PaintResult paint_wet_dry(PaintWetSheet* sheet);
PaintResult paint_wet_dry64(PaintWetSheet64* sheet);

// Transfer dry deposited pigment into allocated pigment tiles and clear it from the sheet.
// Validates the whole rectangle before changing either owner (PE9).
PaintResult paint_wet_deposit(PaintWetSheet* sheet, PaintTiles* layer, PaintRect rect,
    const float* pigments, int32_t pigment_count);

// Validate a transfer and mark its required tiles before either owner is unshared.
PaintResult paint_wet_deposit_bounds(PaintWetSheet* sheet, PaintTiles* layer, PaintRect rect,
    const float* pigments, int32_t pigment_count, uint8_t* touched);

PaintResult paint_wet_deposit_bounds64(PaintWetSheet64* sheet, PaintTiles* layer, PaintRect rect,
    const float* pigments, int32_t pigment_count, uint8_t* touched);
PaintResult paint_wet_deposit64(PaintWetSheet64* sheet, PaintTiles* layer, PaintRect rect,
    const float* pigments, int32_t pigment_count);

// Preflight and apply ordered wet dab batches without retaining caller storage (PE9).
PaintResult paint_wet_dabs_bounds(PaintWetSheet*, const float*, int64_t, int32_t, const PaintDabParams*);
PaintResult paint_wet_dabs(PaintWetSheet*, const float*, int64_t, int32_t, const PaintDabParams*);
PaintResult paint_wet_dabs_bounds64(PaintWetSheet64*, const float*, int64_t, int32_t, const PaintDabParams*);
PaintResult paint_wet_dabs64(PaintWetSheet64*, const float*, int64_t, int32_t, const PaintDabParams*);

// Wet bristle ink has selected-pigment amount, water, and a reserved third field (PE9, PE10).
PaintResult paint_wet_bristles_bounds(PaintWetSheet*, const float*, int64_t, int32_t, float*, int32_t, int32_t, int32_t, const PaintBristleParams*);
PaintResult paint_wet_bristles(PaintWetSheet*, const float*, int64_t, int32_t, float*, int32_t, int32_t, int32_t, const PaintBristleParams*);
PaintResult paint_wet_bristles_bounds64(PaintWetSheet64*, const float*, int64_t, int32_t, float*, int32_t, int32_t, int32_t, const PaintBristleParams*);
PaintResult paint_wet_bristles64(PaintWetSheet64*, const float*, int64_t, int32_t, float*, int32_t, int32_t, int32_t, const PaintBristleParams*);

// Present sheet pigment and the underlying film together without changing either owner (PE8, PE9).
PaintResult paint_wet_present(const PaintWetSheet*, const PaintTiles*, PaintRect, PaintView*, const float*, int32_t, bool, uint64_t);
PaintResult paint_wet_present64(const PaintWetSheet64*, const PaintTiles*, PaintRect, PaintView*, const float*, int32_t, bool, uint64_t);
