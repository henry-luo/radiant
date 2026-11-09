#pragma once
// Context-free painting kernels (Radiant_Design_Paint_Engine PE1): per-pixel loops over
// caller-owned storage views. No GC values, no Lambda runtime or Radiant view/DOM headers,
// and no pointer survives a call (D5.5.2). Brush dynamics and planning live in lambda.paint.
#include <stddef.h>
#include <stdint.h>

enum PaintElem : uint8_t { PAINT_ELEM_U8 = 1, PAINT_ELEM_F32 = 2 };

// rgba: premultiplied RGBA, 4 channels, f32 only. coverage: one channel, u8 or f32.
enum PaintFormat : uint8_t { PAINT_FORMAT_RGBA = 1, PAINT_FORMAT_COVERAGE = 2 };

enum PaintStatus : uint8_t { PAINT_OK = 0, PAINT_ERROR_ARGUMENT = 1, PAINT_ERROR_BUDGET = 2,
    PAINT_ERROR_UNSUPPORTED = 3 };

// dab falloff: 0 is libmypaint's two-segment curve over squared distance; 1 is a linear
// cone with a flat core of radius `hardness` (lambda.image's original brush).
enum PaintProfile : uint8_t { PAINT_PROFILE_MYPAINT = 0, PAINT_PROFILE_LINEAR = 1 };

// separable blend modes for the normal part of a dab (W3C Compositing and Blending).
enum PaintBlend : uint8_t { PAINT_BLEND_NORMAL = 0, PAINT_BLEND_MULTIPLY, PAINT_BLEND_SCREEN,
    PAINT_BLEND_OVERLAY, PAINT_BLEND_DARKEN, PAINT_BLEND_LIGHTEN, PAINT_BLEND_ADD, PAINT_BLEND_COUNT };

// Dab table layout v1 (PE Appendix A): one f32 row per dab.
enum PaintDabColumn {
    PAINT_DAB_X = 0, PAINT_DAB_Y, PAINT_DAB_RADIUS, PAINT_DAB_ASPECT, PAINT_DAB_ANGLE,
    PAINT_DAB_HARDNESS, PAINT_DAB_AA, PAINT_DAB_OPACITY, PAINT_DAB_FLOW,
    PAINT_DAB_R, PAINT_DAB_G, PAINT_DAB_B, PAINT_DAB_ERASER, PAINT_DAB_LOCK_ALPHA,
    PAINT_DAB_COLORIZE, PAINT_DAB_POSTERIZE, PAINT_DAB_POSTERIZE_LEVELS,
    PAINT_DAB_SMUDGE, PAINT_DAB_SMUDGE_LENGTH, PAINT_DAB_SMUDGE_RADIUS, PAINT_DAB_SMUDGE_BUCKET,
    PAINT_DAB_DH, PAINT_DAB_DL, PAINT_DAB_DHSL_S, PAINT_DAB_DV, PAINT_DAB_DHSV_S,
    PAINT_DAB_TIP, PAINT_DAB_GRAIN, PAINT_DAB_PIGMENT, PAINT_DAB_AMOUNT, PAINT_DAB_WATER,
    PAINT_DAB_THICKNESS, PAINT_DAB_PROFILE, PAINT_DAB_BLEND,
    PAINT_DAB_COLUMNS
};
static const int PAINT_DAB_LAYOUT_VERSION = 1;
static const int PAINT_SMUDGE_FIELDS = 4;   // premultiplied r, g, b and alpha per bucket

// One 2-D or 3-D typed-array storage window; channels are contiguous.
struct PaintView {
    void* data;                  // first sample of row 0
    PaintElem elem;
    int32_t width, height, channels;
    int64_t row_stride, pixel_stride;   // in elements
};

// A sparse tiled layer (PT4): tiles_x * tiles_y views in row-major order.
// A view with data == nullptr is an empty tile (transparent rgba, zero coverage).
struct PaintTiles {
    PaintView* tiles;
    int32_t width, height;       // canvas extent in pixels
    int32_t tiles_x, tiles_y, tile_size;
    PaintFormat format;
    PaintElem elem;
};

struct PaintRect { int32_t x0, y0, x1, y1; };   // half-open; empty when x1 <= x0 or y1 <= y0

struct PaintResult {
    PaintStatus status;
    const char* reason;          // static text when status != PAINT_OK
    PaintRect dirty;             // pixels the call could have changed
    uint64_t work;               // candidate pixels charged
};

// What a dab table may reference besides the target (PE4). Every pointer is optional.
struct PaintDabParams {
    uint64_t work_budget;        // candidate pixels this call may charge, pickup included
    const PaintView* tips;       // f32 (h, w) coverage tips, indexed by the `tip` column
    int32_t tip_count;
    const PaintView* paper;      // f32 (h, w) paper height in [0, 1], tiled in canvas space (`grain`)
    float* smudge;               // in/out (buckets, 4) premultiplied bucket colours (`smudge_bucket`)
    int32_t smudge_buckets;
    float* resolved;             // out (count, 4): each dab's colour and target alpha after smudge
};

// Marks in `touched` (tiles_x * tiles_y bytes, caller-zeroed) every tile a dab can write,
// validates every row against `params` and charges the candidate-pixel work (PE3, PE11).
PaintResult paint_dab_bounds(const float* dabs, int64_t count, int32_t columns,
    int32_t width, int32_t height, int32_t tile_size, const PaintDabParams* params, uint8_t* touched);

// Renders the dab table in order (PE4). An rgba target composites each dab (build-up), with
// smudge pickup from the pixels earlier dabs wrote; a coverage target accumulates
// c = 1 - (1 - c)(1 - mask * flow) (wash). Every tile that paint_dab_bounds marked must be
// present; nothing is written if validation fails.
PaintResult paint_dabs(PaintTiles* target, const float* dabs, int64_t count, int32_t columns,
    const PaintDabParams* params);

// libmypaint get_color (PE5): the mask-weighted average under a round footprint, as straight
// [r, g, b, a]; transparent where nothing is painted.
PaintResult paint_sample(const PaintTiles* layer, float x, float y, float radius, float out[4]);

// Wash composite: target = source blended toward the stroke colour by coverage * opacity,
// inside `rect`. target and source may share tiles; source tiles may be empty.
struct PaintWashParams {
    float color[3];              // in the layer's blend space
    float eraser;                // 1 paints, 0 erases (libmypaint alpha_eraser)
    float lock_alpha;            // 0..1
    float opacity;               // stroke opacity, applied once
};
PaintResult paint_wash_composite(PaintTiles* target, const PaintTiles* source,
    const PaintTiles* coverage, PaintRect rect, const PaintWashParams* params);

// Writes straight-alpha u8 or f32 samples (h, w, 3|4) into rgba tiles at `origin`,
// premultiplying; `decode_srgb` converts encoded u8/f32 colour to linear light.
PaintResult paint_import(PaintTiles* target, const PaintView* source, int32_t origin_x,
    int32_t origin_y, bool decode_srgb);

// Converts `rect` of an rgba layer to straight-alpha u8 RGBA in `out` (rect-sized),
// optionally over an opaque or translucent straight-alpha background in the same space.
struct PaintPresentParams {
    bool encode_srgb;            // the layer holds linear light
    bool has_background;
    float background[4];         // straight alpha, layer space
};
PaintResult paint_present(const PaintTiles* layer, PaintRect rect, PaintView* out,
    const PaintPresentParams* params);

// Validates a tile set: geometry, per-tile views and element kinds (PE11).
bool paint_tiles_valid(const PaintTiles* tiles, const char** reason);
