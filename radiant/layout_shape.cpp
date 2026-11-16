#include "shape_exclusion.hpp"
#include "layout.hpp"
#include "render.hpp"
#include "lib/arena.h"
#include <math.h>
#include <string.h>

namespace {

struct ShapeScalar {
    float basis, scale, value;
    bool threshold;
};

bool consume_scalar(DomElement* owner, const CssValue* value, int, void* target) {
    ShapeScalar* scalar = (ShapeScalar*)target;
    CssMathResult math = {};
    if (!css_prop_compute_numeric_value(owner, value, &math, scalar->basis / scalar->scale) ||
        !isfinite(math.value)) return false;
    scalar->value = scalar->threshold ? fminf(fmaxf((float)math.value, 0.0f), 1.0f)
        : fmaxf((float)math.value * scalar->scale, 0.0f);
    return true;
}

struct ShapeBuild {
    LayoutContext* layout;
    BlockContext* context;
    FloatBox* box;
    float margin;
};

Rect reference_box(FloatBox* box, CssEnum kind) {
    ViewBlock* element = box->element;
    BoxMetrics metrics = layout_box_metrics(element);
    Rect rect = {box->margin_box_left + metrics.margin.left,
        box->margin_box_top + metrics.margin.top, element->width, element->height};
    if (kind == CSS_VALUE_MARGIN_BOX) return {box->margin_box_left, box->margin_box_top,
        box->margin_box_right - box->margin_box_left, box->margin_box_bottom - box->margin_box_top};
    if (kind == CSS_VALUE_PADDING_BOX || kind == CSS_VALUE_CONTENT_BOX) {
        rect.x += metrics.border.left; rect.y += metrics.border.top;
        rect.width -= metrics.border_h; rect.height -= metrics.border_v;
    }
    if (kind == CSS_VALUE_CONTENT_BOX) {
        rect.x += metrics.padding.left; rect.y += metrics.padding.top;
        rect.width -= metrics.padding_h; rect.height -= metrics.padding_v;
    }
    rect.width = fmaxf(rect.width, 0.0f); rect.height = fmaxf(rect.height, 0.0f);
    return rect;
}

bool consume_shape(DomElement* owner, const CssValue* value, int, void* target) {
    ShapeBuild* build = (ShapeBuild*)target;
    if (!value || css_value_keyword_equals(value, CSS_VALUE_NONE)) return true;
    const CssValue* syntax = nullptr;
    CssEnum kind;
    if (!css_shape_outside_components(value, &syntax, &kind)) return false;
    Pool* pool = build->context->pool;
    auto* contour = (ShapeFloatContour*)pool_calloc(pool, sizeof(ShapeFloatContour));
    if (!contour) return false;
    contour->margin = build->margin;
    Rect rect = reference_box(build->box, kind);
    CssImageChoice choice = {};
    if (syntax && layout_select_css_image(syntax, layout_image_device_scale(build->layout), &choice) && choice.url) {
        if (!build->layout || !build->layout->ui_context) return false;
        CssDeclaration* declaration = dom_element_get_specified_value(owner, CSS_PROPERTY_SHAPE_OUTSIDE);
        char* source = resolve_css_resource_url(build->layout, declaration, choice.url, true);
        // remote alpha contours require CORS admission from the resource provider.
        if (source && (strncasecmp(source, "http:", 5) == 0 || strncasecmp(source, "https:", 6) == 0)) return false;
        ImageSurface* image = source ? load_document_image(owner->doc, build->layout->ui_context, source) : nullptr;
        if (!image) return false;
        image_surface_ensure_decoded(image, image->width, image->height);
        int width = image->decoded_width > 0 ? image->decoded_width : image->width;
        int height = image->decoded_height > 0 ? image->decoded_height : image->height;
        if (!image->pixels || width <= 0 || height <= 0 || image->pitch <= 0) return false;
        ImagePixelView pixels = {(const uint8_t*)image->pixels, (size_t)image->pitch * (size_t)height,
            (size_t)image->pitch, (uint32_t)width, (uint32_t)height, 4, // INT_CAST_OK: decoded pixel extents are source buffer indices
            image->alpha_mode == IMAGE_ALPHA_PREMULTIPLIED};
        contour->rows = lam::own_arr((ShapeImageRow*)pool_alloc(pool, sizeof(ShapeImageRow) * (size_t)height));
        if (!contour->rows) return false;
        ShapeScalar threshold = {1.0f, 1.0f, 0.0f, true};
        css_prop_consume_cascaded_value(build->box->element, CSS_PROPERTY_SHAPE_IMAGE_THRESHOLD,
            consume_scalar, &threshold);
        if (!shape_exclusion_alpha_rows(&pixels, threshold.value, contour->rows, (size_t)height)) return false;
        contour->row_count = (size_t)height;
        contour->image_rect = reference_box(build->box, CSS_VALUE_CONTENT_BOX);
    } else if (syntax) {
        Arena* arena = arena_create_default();
        if (!arena) return false;
        ScratchArena scratch = {};
        scratch_init(&scratch, arena);
        ScratchMark mark = scratch_scope_begin(&scratch);
        ClipShape* resolved = css_basic_shape_resolve(&scratch, &mark, owner, syntax,
            rect.width, rect.height, rect.x, rect.y, layout_effective_zoom(build->box->element));
        bool valid = resolved != nullptr;
        if (resolved) {
            contour->shape = *resolved;
            if (resolved->type == CLIP_SHAPE_POLYGON) {
                size_t bytes = sizeof(float) * (size_t)resolved->polygon.count;
                contour->shape.polygon.vx = lam::own_arr((float*)pool_alloc(pool, bytes));
                contour->shape.polygon.vy = lam::own_arr((float*)pool_alloc(pool, bytes));
                valid = contour->shape.polygon.vx && contour->shape.polygon.vy;
                if (valid) {
                    memcpy(contour->shape.polygon.vx, resolved->polygon.vx, bytes);
                    memcpy(contour->shape.polygon.vy, resolved->polygon.vy, bytes);
                }
            }
        }
        scratch_scope_end(&scratch, &mark);
        scratch_release(&scratch);
        arena_destroy(arena);
        if (!valid) return false;
    } else {
        contour->box_shape = true;
        contour->box_rect = rect;
        ViewBlock* element = build->box->element;
        BorderProp* border = element->bound ? element->boundary()->border.get() : nullptr;
        if (border) {
            Corner radius = border->radius;
            resolve_border_radius_percentages(&radius, element->width, element->height);
            Rect border_rect = reference_box(build->box, CSS_VALUE_BORDER_BOX);
            contour->box_radius = render_geometry_outset_corners(&radius, element->width, element->height,
                border_rect.y - rect.y, rect.x + rect.width - border_rect.x - border_rect.width,
                rect.y + rect.height - border_rect.y - border_rect.height, border_rect.x - rect.x);
        }
    }
    build->box->shape_contour = lam::up(contour);
    return true;
}

} // namespace

void shape_float_build(LayoutContext* layout, BlockContext* context, FloatBox* box) {
    if (!context || !box || !box->element || !context->pool ||
        layout_block_inline_axis_is_vertical(context->establishing_element)) return;
    ViewBlock* flow = layout_nearest_block_ancestor(box->element->parent_view());
    // a parent's measured content_width is published after its floats are registered.
    float basis = flow ? layout_content_box(flow).width : context->content_width;
    ShapeScalar margin = {basis, layout_effective_zoom(box->element), 0.0f, false};
    css_prop_consume_cascaded_value(box->element, CSS_PROPERTY_SHAPE_MARGIN, consume_scalar, &margin);
    ShapeBuild build = {layout, context, box, margin.value};
    css_prop_consume_cascaded_value(box->element, CSS_PROPERTY_SHAPE_OUTSIDE, consume_shape, &build);
}

bool shape_float_band(const FloatBox* box, float top, float bottom, float* left, float* right) {
    if (!box || !box->shape_contour) return false;
    const ShapeFloatContour* contour = box->shape_contour;
    top = fmaxf(top, box->margin_box_top);
    bottom = fminf(bottom, box->margin_box_bottom);
    bool intersects = contour->box_shape
        ? shape_exclusion_box_band(contour->box_rect, &contour->box_radius,
            top, bottom, contour->margin, left, right)
        : contour->row_count
        ? shape_exclusion_image_band(contour->rows, contour->row_count, contour->image_rect,
            top, bottom, contour->margin, left, right)
        : shape_exclusion_band(&contour->shape, top, bottom, contour->margin, left, right);
    if (!intersects) return false;
    *left = fmaxf(*left, box->margin_box_left);
    *right = fminf(*right, box->margin_box_right);
    return *left <= *right;
}

void shape_float_offset(FloatBox* box, float dx, float dy) {
    if (!box || !box->shape_contour) return;
    ShapeFloatContour* contour = box->shape_contour;
    if (contour->box_shape) {
        contour->box_rect.x += dx;
        contour->box_rect.y += dy;
    } else if (contour->row_count) {
        contour->image_rect.x += dx;
        contour->image_rect.y += dy;
    } else clip_shape_offset(&contour->shape, dx, dy);
}
