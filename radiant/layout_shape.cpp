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

struct ShapeAxes {
    bool vertical, reverse_inline, reverse_block;
    float width, height, origin_x, origin_y;

    RdtLogicalPoint point(float x, float y) const {
        return vertical ? RdtLogicalPoint{origin_x + (reverse_inline ? height - y : y),
            origin_y + (reverse_block ? width - x : x)} : RdtLogicalPoint{origin_x + x, origin_y + y};
    }
    Rect rect(Rect physical) const {
        RdtLogicalPoint a = point(physical.x, physical.y);
        RdtLogicalPoint b = point(physical.x + physical.width, physical.y + physical.height);
        return {fminf(a.x, b.x), fminf(a.y, b.y), fabsf(b.x - a.x), fabsf(b.y - a.y)};
    }
    Corner corners(const Corner& physical) const {
        if (!vertical) return physical;
        Corner result = {};
        for (int corner = 0; corner < 4; corner++) {
            bool right = corner == 1 || corner == 2, bottom = corner >= 2;
            bool flow_right = bottom != reverse_inline, flow_bottom = right != reverse_block;
            int target = flow_bottom ? (flow_right ? 2 : 3) : (flow_right ? 1 : 0);
            result.horizontal[target] = physical.vertical[corner];
            result.vertical[target] = physical.horizontal[corner];
        }
        return result;
    }
};

ShapeAxes shape_axes(FloatBox* box) {
    ViewBlock* element = box->element;
    ViewBlock* flow = layout_nearest_block_ancestor(element->parent_view());
    BoxEdges margin = layout_line_relative_margin_edges(element, flow);
    return {layout_block_inline_axis_is_vertical(flow), !layout_line_left_is_physical_start(flow),
        layout_block_writing_mode(flow) == WM_VERTICAL_RL, element->width, element->height,
        box->margin_box_left + margin.left, box->margin_box_top + margin.top};
}

void map_shape_to_flow(ShapeFloatContour* contour, const ShapeAxes& axes) {
    if (contour->box_shape) {
        contour->box_rect = axes.rect(contour->box_rect);
        contour->box_radius = axes.corners(contour->box_radius);
    } else if (contour->row_count) {
        contour->image_rect = axes.rect(contour->image_rect);
    } else {
        ClipShape* shape = &contour->shape;
        switch (shape->type) {
            case CLIP_SHAPE_CIRCLE: {
                RdtLogicalPoint center = axes.point(shape->circle.cx, shape->circle.cy);
                shape->circle.cx = center.x; shape->circle.cy = center.y;
                break;
            }
            case CLIP_SHAPE_ELLIPSE: {
                auto source = shape->ellipse;
                RdtLogicalPoint center = axes.point(source.cx, source.cy);
                shape->ellipse = {center.x, center.y, axes.vertical ? source.ry : source.rx,
                    axes.vertical ? source.rx : source.ry};
                break;
            }
            case CLIP_SHAPE_INSET: {
                auto source = shape->inset;
                Rect rect = axes.rect({source.x, source.y, source.w, source.h});
                shape->inset = {rect.x, rect.y, rect.width, rect.height, 0, 0};
                break;
            }
            case CLIP_SHAPE_ROUNDED_RECT: {
                auto source = shape->rounded_rect;
                Corner radius = render_path_uniform_corner(source.r_tl, source.r_tr, source.r_br, source.r_bl);
                contour->box_shape = true;
                contour->box_rect = axes.rect({source.x, source.y, source.w, source.h});
                contour->box_radius = axes.corners(radius);
                break;
            }
            case CLIP_SHAPE_POLYGON:
                for (int vertex = 0; vertex < shape->polygon.count; vertex++) {
                    RdtLogicalPoint point = axes.point(shape->polygon.vx[vertex], shape->polygon.vy[vertex]);
                    shape->polygon.vx[vertex] = point.x; shape->polygon.vy[vertex] = point.y;
                }
                break;
            default: break;
        }
    }
}

Rect reference_box(FloatBox* box, CssEnum kind) {
    ViewBlock* element = box->element;
    BoxMetrics metrics = layout_box_metrics(element);
    // basic shape syntax uses physical axes before conversion to the BFC line frame.
    Rect rect = {0, 0, element->width, element->height};
    if (kind == CSS_VALUE_MARGIN_BOX) return {-metrics.margin.left, -metrics.margin.top,
        element->width + metrics.margin_h, element->height + metrics.margin_v};
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

bool copy_image_contour(ShapeBuild* build, ShapeFloatContour* contour,
        const ShapeAxes& axes, ImageSurface* image) {
    Pool* pool = build->context->pool;
    int width = image->decoded_width > 0 ? image->decoded_width : image->width;
    int height = image->decoded_height > 0 ? image->decoded_height : image->height;
    if (!image->pixels || width <= 0 || height <= 0 || image->pitch <= 0) return false;
    ImagePixelView pixels = {(const uint8_t*)image->pixels, (size_t)image->pitch * (size_t)height,
        (size_t)image->pitch, (uint32_t)width, (uint32_t)height, 4, // INT_CAST_OK: decoded pixel extents are source buffer indices
        image->alpha_mode == IMAGE_ALPHA_PREMULTIPLIED};
    size_t rows = axes.vertical ? (size_t)width : (size_t)height;
    contour->rows = lam::own_arr((ShapeImageRow*)pool_alloc(pool, sizeof(ShapeImageRow) * rows));
    if (!contour->rows) return false;
    ShapeScalar threshold = {1.0f, 1.0f, 0.0f, true};
    css_prop_consume_cascaded_value(build->box->element, CSS_PROPERTY_SHAPE_IMAGE_THRESHOLD,
        consume_scalar, &threshold);
    if (!shape_exclusion_alpha_rows(&pixels, threshold.value, contour->rows, rows,
            axes.vertical, axes.vertical && axes.reverse_inline, axes.vertical && axes.reverse_block)) return false;
    contour->row_count = rows;
    contour->image_rect = reference_box(build->box, CSS_VALUE_CONTENT_BOX);
    return true;
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
    ShapeAxes axes = shape_axes(build->box);
    CssImageChoice choice = {};
    if (syntax && layout_select_css_image(syntax, layout_image_device_scale(build->layout), &choice) && choice.url) {
        if (!build->layout || !build->layout->ui_context) return false;
        CssDeclaration* declaration = dom_element_get_specified_value(owner, CSS_PROPERTY_SHAPE_OUTSIDE);
        char* source = resolve_css_resource_url(build->layout, declaration, choice.url, true);
        // remote alpha contours require CORS admission from the resource provider.
        if (source && (strncasecmp(source, "http:", 5) == 0 || strncasecmp(source, "https:", 6) == 0)) return false;
        ImageSurface* image = source ? load_document_image(owner->doc, build->layout->ui_context, source) : nullptr;
        if (!image) return false;
        // vector resources have no decoded alpha until the shared SVG rasterizer runs.
        if (image->format == IMAGE_FORMAT_SVG) {
            if (!render_media_rasterize_svg_picture(image, image->width, image->height)) return false;
        } else {
            image_surface_ensure_decoded(image, image->width, image->height);
        }
        if (!copy_image_contour(build, contour, axes, image)) return false;
    } else if (syntax && (css_value_image_kind(syntax) == CSS_IMAGE_LINEAR_GRADIENT ||
            css_value_image_kind(syntax) == CSS_IMAGE_RADIAL_GRADIENT ||
            css_value_image_kind(syntax) == CSS_IMAGE_CONIC_GRADIENT)) {
        ListStyleImage gradient = {};
        if (!layout_resolve_gradient_image(build->layout, syntax, &gradient)) return false;
        Rect content = reference_box(build->box, CSS_VALUE_CONTENT_BOX);
        ImageSurface* image = render_gradient_image_rasterize(&gradient, {0, 0, content.width, content.height}, 1.0f);
        if (!image) return false;
        bool valid = copy_image_contour(build, contour, axes, image);
        image_surface_destroy(image);
        if (!valid) return false;
    } else if (syntax && css_basic_shape_inset_geometry(owner, syntax, rect.width, rect.height,
            rect.x, rect.y, layout_effective_zoom(build->box->element),
            &contour->box_rect, &contour->box_radius)) {
        contour->box_shape = true;
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
    map_shape_to_flow(contour, axes);
    build->box->shape_contour = lam::up(contour);
    return true;
}

} // namespace

void shape_float_build(LayoutContext* layout, BlockContext* context, FloatBox* box) {
    if (!context || !box || !box->element || !context->pool) return;
    ViewBlock* flow = layout_nearest_block_ancestor(box->element->parent_view());
    // a parent's measured content_width is published after its floats are registered.
    bool vertical = layout_block_inline_axis_is_vertical(flow);
    LayoutContentBox content = flow ? layout_content_box(flow) : LayoutContentBox{};
    float basis = flow ? (vertical ? content.height : content.width) : context->content_width;
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
