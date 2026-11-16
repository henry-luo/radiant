#pragma once

#include "view.hpp"
#include "lib/image.h"

// bounds of a shape expanded by margin over an entire half-open line band.
// coordinates are physical CSS pixels; callers clip to the float margin box.
bool shape_exclusion_band(const ClipShape* shape, float top, float bottom,
    float margin, float* left, float* right);
bool shape_exclusion_box_band(const Rect& rect, const Corner* radius,
    float top, float bottom, float margin, float* left, float* right);

struct ShapeImageRow {
    float left, right; // normalized source edges; left > right denotes an empty row
};

// copy alpha-derived outer edges into caller-owned rows; no pixel storage is retained.
bool shape_exclusion_alpha_rows(const ImagePixelView* image, float threshold,
    ShapeImageRow* rows, size_t capacity, bool transpose = false,
    bool reverse_x = false, bool reverse_y = false);
bool shape_exclusion_image_band(const ShapeImageRow* rows, size_t count,
    const Rect& image_rect, float top, float bottom, float margin, float* left, float* right);

struct ShapeFloatContour {
    ClipShape shape;
    Rect box_rect;
    Corner box_radius;
    bool box_shape;
    lam::OwnArr<ShapeImageRow> rows;
    size_t row_count;
    Rect image_rect;
    float margin;
};
LAM_NODE_OF(ShapeFloatContour, NodeStack);

struct FloatBox;
struct BlockContext;
void shape_float_build(LayoutContext* layout, BlockContext* context, FloatBox* box);
bool shape_float_band(const FloatBox* box, float top, float bottom, float* left, float* right);
void shape_float_offset(FloatBox* box, float dx, float dy);
