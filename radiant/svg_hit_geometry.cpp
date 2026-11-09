#include "svg_hit.hpp"
#include "svg_geometry_context.hpp"
#include "svg_animation.hpp"
#include "event.hpp"
#include "layout.hpp"
#include "render.hpp"
#include "view.hpp"
#include "radiant.hpp"
#include "tagged.hpp"
#include "../lambda/input/css/dom_element.hpp"
#include "../lambda/input/css/dom_svg_context.h"
#include "../lib/math_utils.h"
#include "../lib/mem.h"
#include "../lib/str.h"
#include <math.h>
#include <string.h>

static const char* dom_svg_presentation_value(DomElement* elem, const char* name,
    bool inherits, char* buffer, size_t buffer_size) {
    return svg_get_dom_presentation_property(elem, name, inherits, buffer, buffer_size,
        nullptr, nullptr, nullptr, g_dom_svg_style_scope);
}

static float dom_svg_presentation_number(DomElement* elem, const char* name,
    float fallback, bool inherits) {
    char value_buffer[64];
    DomElement* declaration = nullptr;
    const char* value = svg_get_dom_presentation_property(elem, name, inherits, value_buffer,
        sizeof(value_buffer), nullptr, nullptr, &declaration, g_dom_svg_style_scope);
    SvgLengthContext lengths = dom_svg_length_context(declaration ? declaration : elem);
    SvgLengthContext used = dom_svg_length_context(elem);
    lengths.viewport_width = used.viewport_width; lengths.viewport_height = used.viewport_height;
    return svg_resolve_length(value, &lengths, dom_svg_length_axis(name), fallback);
}

static bool dom_svg_paint_is_present(DomElement* elem, const char* paint_name,
                                        bool paint_is_present_by_default) {
    if (!elem || !paint_name) return false;
    char paint_buffer[256] = {};
    const char* paint = dom_svg_presentation_value(elem, paint_name, true,
        paint_buffer, sizeof(paint_buffer));
    if (paint && (!*paint || str_icmp_cstr(paint, "none") == 0)) return false;
    return paint || paint_is_present_by_default;
}

typedef struct NativeSvgShapeHit {
    bool fill;
    bool stroke;
    bool bounding_box;
    bool fill_painted;
    bool stroke_painted;
    DomElement* instance_source;
    DomElement* instance_hosts[16];
    unsigned instance_depth;
} NativeSvgShapeHit;

static void dom_svg_configure_stroke_hit(DomElement* elem, SvgPathHitContext* context) {
    if (!context) return;
    float stroke_width = dom_svg_presentation_number(elem, "stroke-width", 1.0f, true);
    context->stroke_enabled = stroke_width > 0.0f;
    if (!context->stroke_enabled) return;
    // SVG pointer-events uses painted geometry; a device-space aiming expansion picks blank pixels.
    context->stroke_radius = stroke_width * 0.5f;
    context->stroke_cap = RDT_CAP_BUTT;
    context->stroke_join = RDT_JOIN_MITER;
    context->stroke_miter_limit = 4.0f;
    char cap_buffer[64] = {};
    char join_buffer[64] = {};
    const char* cap = dom_svg_presentation_value(elem, "stroke-linecap", true,
        cap_buffer, sizeof(cap_buffer));
    const char* join = dom_svg_presentation_value(elem, "stroke-linejoin", true,
        join_buffer, sizeof(join_buffer));
    if (cap && str_icmp_cstr(cap, "round") == 0) context->stroke_cap = RDT_CAP_ROUND;
    else if (cap && str_icmp_cstr(cap, "square") == 0) context->stroke_cap = RDT_CAP_SQUARE;
    if (join && str_icmp_cstr(join, "round") == 0) context->stroke_join = RDT_JOIN_ROUND;
    else if (join && str_icmp_cstr(join, "bevel") == 0) context->stroke_join = RDT_JOIN_BEVEL;
    context->stroke_miter_limit = dom_svg_presentation_number(elem,
        "stroke-miterlimit", 4.0f, true);
    if (context->stroke_miter_limit < 1.0f) context->stroke_miter_limit = 1.0f;
    context->stroke_dash_offset = dom_svg_presentation_number(elem,
        "stroke-dashoffset", 0.0f, true);
    char* dasharray = nullptr;
    DomElement* declaration = nullptr;
    svg_get_dom_presentation_property(elem, "stroke-dasharray", true, nullptr, 0,
        nullptr, &dasharray, &declaration, g_dom_svg_style_scope);
    SvgLengthContext lengths = dom_svg_length_context(declaration ? declaration : elem);
    SvgLengthContext used = dom_svg_length_context(elem);
    lengths.viewport_width = used.viewport_width; lengths.viewport_height = used.viewport_height;
    context->stroke_dash_count = svg_resolve_dash_array(dasharray, &lengths, nullptr, 0);
    if (context->stroke_dash_count > 0) {
        context->stroke_dash = (float*)mem_alloc((size_t)context->stroke_dash_count * sizeof(float), MEM_CAT_RENDER);
        context->stroke_dash_count = context->stroke_dash
            ? svg_resolve_dash_array(dasharray, &lengths, context->stroke_dash, context->stroke_dash_count) : 0;
    }
    mem_free(dasharray);
    for (int index = 0; index < context->stroke_dash_count; index++) {
        context->stroke_dash_total += context->stroke_dash[index];
    }
    if (context->stroke_dash_total <= 0.0f) context->stroke_dash_count = 0;
}

static NativeSvgShapeHit dom_svg_basic_shape_hit_local_point(DomElement* elem,
                                                               float local_x,
                                                               float local_y,
                                                               float min_scale,
                                                               const RdtMatrix* stroke_frame = nullptr,
                                                               bool check_stroke = true) {
    NativeSvgShapeHit result = {};
    if (!elem) return result;
    // Every basic SVG primitive is lowered to the same contour visitor.  Its
    // bounding box contains non-painted gaps, so it cannot decide a hit.
    bool text = dom_svg_is_text(elem);
    RdtPath* path = text ? dom_svg_text_geometry_path(elem) : dom_svg_basic_shape_path(elem);
    if (!path) return result;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    result.bounding_box = rdt_path_get_bounds(path, &left, &top, &right, &bottom) &&
        local_x >= left && local_x <= right && local_y >= top && local_y <= bottom;
    // fill-only hits cannot reach beyond the contour bounds; a box miss avoids
    // flattening curves and resolving the unused stroke/dash cascade.
    if (!check_stroke && !result.bounding_box) { rdt_path_free(path); return result; }
    SvgPathHitContext context = {};
    context.point_x = local_x;
    context.point_y = local_y;
    context.curve_flatness = 0.5f / min_scale;
    char effect_buffer[64];
    const char* effect = check_stroke
        ? dom_svg_presentation_value(elem, "vector-effect", false, effect_buffer, sizeof(effect_buffer)) : nullptr;
    bool non_scaling = !text && stroke_frame && effect && strcmp(effect, "non-scaling-stroke") == 0;
    if (check_stroke && !non_scaling) dom_svg_configure_stroke_hit(elem, &context);
    bool visited = rdt_path_visit(path, dom_svg_path_hit_visit, &context);
    if (visited) dom_svg_path_hit_finish_subpath(&context);
    if (!visited) { mem_free(context.stroke_dash); rdt_path_free(path); return result; }
    {
        char fill_rule_buffer[64] = {};
        const char* fill_rule = dom_svg_presentation_value(elem, "fill-rule", true,
            fill_rule_buffer, sizeof(fill_rule_buffer));
        bool even_odd = fill_rule && str_icmp_cstr(fill_rule, "evenodd") == 0;
        if (context.fill_on_edge || (even_odd
                ? (context.fill_crossings & 1) != 0
                : context.fill_winding != 0)) {
            result.fill = true;
        }
    }
    // SVG text fill and stroke targeting both use the character cell (SVG 2 §17.5.1).
    result.stroke = context.stroke_enabled && (text ? result.fill : context.stroke_hit);
    if (non_scaling) {
        RdtPath* transformed = rdt_path_new();
        if (transformed && render_path_append_transformed(transformed, path, stroke_frame)) {
            SvgPathHitContext target = {};
            rdt_matrix_transform_point(stroke_frame, local_x, local_y, &target.point_x, &target.point_y);
            target.curve_flatness = 0.5f;
            // the same affine contour as painting is stroked in CSS viewport units.
            dom_svg_configure_stroke_hit(elem, &target);
            bool inspected = rdt_path_visit(transformed, dom_svg_path_hit_visit, &target);
            if (inspected) dom_svg_path_hit_finish_subpath(&target);
            result.stroke = inspected && target.stroke_enabled && target.stroke_hit;
            mem_free(target.stroke_dash);
        }
        if (transformed) rdt_path_free(transformed);
    }
    mem_free(context.stroke_dash);
    rdt_path_free(path);
    return result;
}

static NativeSvgShapeHit dom_svg_basic_shape_hit_viewport_point(DomElement* elem,
                                                                  float viewport_x,
                                                                  float viewport_y, bool check_stroke) {
    NativeSvgShapeHit result = {};
    if (!elem) return result;
    RdtMatrix screen_ctm = dom_svg_ctm(elem, true);
    float local_x = 0.0f;
    float local_y = 0.0f;
    if (!rdt_matrix_unproject_affine_point(&screen_ctm, viewport_x, viewport_y,
                                           &local_x, &local_y)) {
        return result;
    }
    float scale_x = hypotf(screen_ctm.e11, screen_ctm.e21);
    float scale_y = hypotf(screen_ctm.e12, screen_ctm.e22);
    float min_scale = LMB_MIN(scale_x, scale_y);
    if (min_scale < 0.0001f) min_scale = 0.0001f;
    return dom_svg_basic_shape_hit_local_point(elem, local_x, local_y, min_scale, &screen_ctm, check_stroke);
}
static bool dom_svg_viewport_local_bounds(DomElement* elem, float* left, float* top,
                                             float* right, float* bottom) {
    if (!dom_svg_tag_is(elem, "svg") || !left || !top || !right || !bottom) return false;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    const char* viewbox = svg_animation_attribute(elem, "viewBox");
    if (!viewbox) viewbox = svg_animation_attribute(elem, "viewbox");
    if (dom_svg_parse_viewbox(viewbox, &min_x, &min_y, &width, &height)) {
        *left = min_x;
        *top = min_y;
        *right = min_x + width;
        *bottom = min_y + height;
        return true;
    }
    width = dom_svg_attribute_number(elem, "width", elem->width);
    height = dom_svg_attribute_number(elem, "height", elem->height);
    dom_svg_apply_css_viewport_size(elem, &width, &height);
    if (width <= 0.0f || height <= 0.0f) return false;
    *left = 0.0f;
    *top = 0.0f;
    *right = width;
    *bottom = height;
    return true;
}

static NativeSvgShapeHit dom_svg_bounds_hit_viewport_point(DomElement* elem,
                                                              float viewport_x,
                                                              float viewport_y) {
    NativeSvgShapeHit result = {};
    if (!elem) return result;
    RdtMatrix screen_ctm = dom_svg_ctm(elem, true);
    float local_x = 0.0f;
    float local_y = 0.0f;
    if (!rdt_matrix_unproject_affine_point(&screen_ctm, viewport_x, viewport_y,
                                           &local_x, &local_y)) {
        return result;
    }
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    if (dom_svg_tag_is(elem, "svg")) {
        if (!dom_svg_viewport_local_bounds(elem, &left, &top, &right, &bottom)) {
            return result;
        }
    } else {
        SvgGeometryBounds bounds = dom_svg_bounds_for_element(elem);
        if (!bounds.valid) return result;
        left = bounds.left;
        top = bounds.top;
        right = bounds.right;
        bottom = bounds.bottom;
    }
    result.bounding_box = local_x >= left && local_x <= right &&
        local_y >= top && local_y <= bottom;
    // Image/text/foreignObject content is painted by its own renderer. Its
    // exposed SVG geometry is its viewport box, not a union of child boxes.
    result.fill = result.bounding_box;
    return result;
}

static NativeSvgShapeHit dom_svg_use_instance_hit(DomElement* elem,
    const RdtMatrix* instance_ctm, float viewport_x, float viewport_y);
static bool dom_svg_element_skips_hit_test(DomElement* elem);

static bool dom_svg_clip_path_contains(const RdtPath* path, const RdtMatrix* frame, RdtFillRule rule, const RdtLogicalPoint* point) {
    RdtPath* contour = rdt_path_new();
    if (!contour || !render_path_append_transformed(contour, path, frame)) {
        if (contour) rdt_path_free(contour);
        return false;
    }
    SvgPathHitContext query = {};
    query.point_x = point->x; query.point_y = point->y; query.curve_flatness = .05f;
    bool valid = rdt_path_visit(contour, dom_svg_path_hit_visit, &query);
    if (valid) dom_svg_path_hit_finish_subpath(&query);
    bool hit = valid && (query.fill_on_edge || (rule == RDT_FILL_EVEN_ODD ? (query.fill_crossings & 1) != 0 : query.fill_winding != 0));
    rdt_path_free(contour);
    return hit;
}

static bool dom_svg_clip_contains(DomElement* elem, const RdtMatrix* frame, float x, float y) {
    char buffer[512];
    const char* clip = dom_svg_presentation_value(elem, "clip-path", false, buffer, sizeof(buffer));
    if (!clip || strcmp(clip, "none") == 0) return true;
    // resolve ancestor transforms only when there is an actual clipping path.
    RdtMatrix computed_frame;
    if (!frame) { computed_frame = dom_svg_ctm(elem, true); frame = &computed_frame; }
    SvgLengthContext lengths = dom_svg_length_context(elem);
    UiContext* ui = elem->doc ? (UiContext*)elem->doc->js.host_ui_context : nullptr;
    RdtLogicalPoint point = {x, y};
    return svg_dom_clip_contains_point(elem, &lengths, ui ? ui->font_ctx : nullptr, frame,
        dom_svg_clip_path_contains, &point, g_dom_svg_style_scope);
}

static NativeSvgShapeHit dom_svg_reference_hit_viewport_point(DomElement* reference,
    const RdtMatrix* reference_ctm, float viewport_x, float viewport_y) {
    NativeSvgShapeHit result = {};
    if (!reference || !reference_ctm || dom_svg_element_skips_hit_test(reference) ||
        !dom_svg_clip_contains(reference, reference_ctm, viewport_x, viewport_y)) return result;
    if (dom_svg_is_basic_shape(reference) || dom_svg_is_text(reference)) {
        float local_x = 0.0f, local_y = 0.0f;
        if (!rdt_matrix_unproject_affine_point(reference_ctm, viewport_x, viewport_y, &local_x, &local_y)) return result;
        float min_scale = LMB_MAX(0.0001f, LMB_MIN(hypotf(reference_ctm->e11, reference_ctm->e21),
            hypotf(reference_ctm->e12, reference_ctm->e22)));
        result = dom_svg_basic_shape_hit_local_point(reference, local_x, local_y, min_scale, reference_ctm);
        result.fill_painted = result.fill && dom_svg_paint_is_present(reference, "fill", true);
        result.stroke_painted = result.stroke && dom_svg_paint_is_present(reference, "stroke", false);
        if (result.fill_painted || result.stroke_painted) result.instance_source = reference;
        return result;
    }
    if (dom_svg_tag_is(reference, "use"))
        return dom_svg_use_instance_hit(reference, reference_ctm, viewport_x, viewport_y);
    if (!dom_svg_tag_is(reference, "g") && !dom_svg_tag_is(reference, "a") &&
        !dom_svg_tag_is(reference, "symbol") && !dom_svg_tag_is(reference, "switch")) return result;
    for (DomNode* child = reference->last_child; child; child = child->prev_sibling) {
        if (!child->is_element()) continue;
        if (dom_svg_tag_is(reference, "switch") && child != dom_svg_switch_selected_child(reference)) continue;
        DomElement* child_elem = child->as_element();
        RdtMatrix child_transform = dom_svg_transform_from_element(child_elem);
        RdtMatrix child_ctm = rdt_matrix_multiply(reference_ctm, &child_transform);
        NativeSvgShapeHit hit = dom_svg_reference_hit_viewport_point(child_elem, &child_ctm, viewport_x, viewport_y);
        result.fill |= hit.fill; result.stroke |= hit.stroke; result.bounding_box |= hit.bounding_box;
        result.fill_painted |= hit.fill_painted; result.stroke_painted |= hit.stroke_painted;
        if (!result.instance_source && hit.instance_source) {
            result.instance_source = hit.instance_source;
            result.instance_depth = hit.instance_depth;
            memcpy(result.instance_hosts, hit.instance_hosts, hit.instance_depth * sizeof(*hit.instance_hosts));
        }
    }
    return result;
}

static NativeSvgShapeHit dom_svg_use_instance_hit(DomElement* elem,
    const RdtMatrix* instance_ctm, float viewport_x, float viewport_y) {
    NativeSvgShapeHit result = {};
    DomElement* reference = dom_svg_use_reference(elem);
    if (!reference || !instance_ctm) return result;
    size_t depth = 0;
    for (const SvgDomStyleScope* scope = g_dom_svg_style_scope; scope; scope = scope->previous) {
        if (scope->root == reference || ++depth >= 16) return result;
    }
    for (DomNode* node = elem; node; node = node->parent) if (node == reference) return result;
    SvgLengthContext lengths = dom_svg_length_context(elem);
    RdtMatrix offset = rdt_matrix_translate(dom_svg_attribute_number(elem, "x", 0.0f),
        dom_svg_attribute_number(elem, "y", 0.0f));
    RdtMatrix frame = rdt_matrix_multiply(instance_ctm, &offset);
    SvgDomStyleScope scope = {lam::up(reference), lam::up(elem), lam::up(g_dom_svg_style_scope), 0.0f, 0.0f, false};
    SvgAnimationSourceScope animation_scope(reference->doc, reference, elem);
    const SvgDomStyleScope* saved_scope = g_dom_svg_style_scope;
    g_dom_svg_style_scope = &scope;
    RdtMatrix local = dom_svg_transform_from_element(reference);
    frame = rdt_matrix_multiply(&frame, &local);
    if (dom_svg_tag_is(reference, "symbol")) {
        SvgViewBox box = svg_parse_viewbox(svg_animation_attribute(reference, "viewBox"));
        float width = svg_resolve_length(svg_animation_attribute(elem, "width"), &lengths, SVG_LENGTH_X, lengths.viewport_width);
        float height = svg_resolve_length(svg_animation_attribute(elem, "height"), &lengths, SVG_LENGTH_Y, lengths.viewport_height);
        if (box.has_viewbox && box.width > 0.0f && box.height > 0.0f && width > 0.0f && height > 0.0f) {
            RdtMatrix mapping = svg_viewbox_transform(&box, width, height, svg_animation_attribute(reference, "preserveAspectRatio"));
            frame = rdt_matrix_multiply(&frame, &mapping);
            scope.has_viewport = true; scope.viewport_width = box.width; scope.viewport_height = box.height;
        }
    }
    result = dom_svg_reference_hit_viewport_point(reference, &frame, viewport_x, viewport_y);
    if (result.instance_source && result.instance_depth < 16)
        result.instance_hosts[result.instance_depth++] = elem;
    SvgGeometryBounds bounds = dom_svg_bounds_for_element(reference);
    float local_x = 0.0f, local_y = 0.0f;
    if (bounds.valid && rdt_matrix_unproject_affine_point(&frame, viewport_x, viewport_y, &local_x, &local_y))
        result.bounding_box = local_x >= bounds.left && local_x <= bounds.right &&
            local_y >= bounds.top && local_y <= bounds.bottom;
    g_dom_svg_style_scope = saved_scope;
    return result;
}

static NativeSvgShapeHit dom_svg_use_hit_viewport_point(DomElement* elem, float viewport_x, float viewport_y) {
    RdtMatrix frame = dom_svg_ctm(elem, true);
    return dom_svg_use_instance_hit(elem, &frame, viewport_x, viewport_y);
}

static void dom_svg_dispatch_use_timing_hit(const NativeSvgShapeHit* hit, unsigned depth,
    const char* type, bool bubbles, double detail) {
    if (!depth) return;
    DomElement* host = hit->instance_hosts[depth - 1];
    if (depth == 1) {
        svg_animation_use_event(host, hit->instance_source, type, bubbles, detail);
        return;
    }
    DomElement* reference = dom_svg_use_reference(host);
    if (!reference) return;
    // replay the hit's instance chain so identical nested source nodes retain distinct timing owners.
    SvgAnimationSourceScope scope(reference->doc, reference, host);
    dom_svg_dispatch_use_timing_hit(hit, depth - 1, type, bubbles, detail);
}

extern "C" void dom_svg_dispatch_use_timing_event(void* element, float x, float y, const char* type, bool bubbles, double detail) {
    DomElement* use = (DomElement*)element;
    if (!use || !dom_svg_tag_is(use, "use")) return;
    NativeSvgShapeHit hit = dom_svg_use_hit_viewport_point(use, x, y);
    if (hit.instance_source) dom_svg_dispatch_use_timing_hit(&hit, hit.instance_depth, type, bubbles, detail);
}

typedef enum NativeSvgPointerEventsMode {
    NATIVE_SVG_POINTER_EVENTS_VISIBLE_PAINTED,
    NATIVE_SVG_POINTER_EVENTS_VISIBLE_FILL,
    NATIVE_SVG_POINTER_EVENTS_VISIBLE_STROKE,
    NATIVE_SVG_POINTER_EVENTS_VISIBLE,
    NATIVE_SVG_POINTER_EVENTS_PAINTED,
    NATIVE_SVG_POINTER_EVENTS_FILL,
    NATIVE_SVG_POINTER_EVENTS_STROKE,
    NATIVE_SVG_POINTER_EVENTS_ALL,
    NATIVE_SVG_POINTER_EVENTS_BOUNDING_BOX,
    NATIVE_SVG_POINTER_EVENTS_NONE,
} NativeSvgPointerEventsMode;

static NativeSvgPointerEventsMode dom_svg_pointer_events_mode(DomElement* elem) {
    char pointer_events_buffer[64] = {};
    const char* value = dom_svg_presentation_value(elem, "pointer-events", true,
        pointer_events_buffer, sizeof(pointer_events_buffer));
    if (!value || !*value || str_icmp_cstr(value, "auto") == 0 ||
        str_icmp_cstr(value, "visiblePainted") == 0) {
        return NATIVE_SVG_POINTER_EVENTS_VISIBLE_PAINTED;
    }
    if (str_icmp_cstr(value, "visibleFill") == 0) return NATIVE_SVG_POINTER_EVENTS_VISIBLE_FILL;
    if (str_icmp_cstr(value, "visibleStroke") == 0) return NATIVE_SVG_POINTER_EVENTS_VISIBLE_STROKE;
    if (str_icmp_cstr(value, "visible") == 0) return NATIVE_SVG_POINTER_EVENTS_VISIBLE;
    if (str_icmp_cstr(value, "painted") == 0) return NATIVE_SVG_POINTER_EVENTS_PAINTED;
    if (str_icmp_cstr(value, "fill") == 0) return NATIVE_SVG_POINTER_EVENTS_FILL;
    if (str_icmp_cstr(value, "stroke") == 0) return NATIVE_SVG_POINTER_EVENTS_STROKE;
    if (str_icmp_cstr(value, "all") == 0) return NATIVE_SVG_POINTER_EVENTS_ALL;
    if (str_icmp_cstr(value, "bounding-box") == 0) return NATIVE_SVG_POINTER_EVENTS_BOUNDING_BOX;
    if (str_icmp_cstr(value, "none") == 0) return NATIVE_SVG_POINTER_EVENTS_NONE;
    return NATIVE_SVG_POINTER_EVENTS_VISIBLE_PAINTED;
}

static bool dom_svg_element_is_visible_for_pointer_events(DomElement* elem) {
    char visibility_buffer[64] = {};
    const char* visibility = dom_svg_presentation_value(elem, "visibility", true,
        visibility_buffer, sizeof(visibility_buffer));
    return !visibility || (str_icmp_cstr(visibility, "hidden") != 0 &&
        str_icmp_cstr(visibility, "collapse") != 0);
}

static bool dom_svg_pointer_events_selects_geometry(NativeSvgPointerEventsMode mode,
                                                        bool visible,
                                                        bool fill_painted,
                                                        bool stroke_painted,
                                                        const NativeSvgShapeHit* hit) {
    if (!hit || mode == NATIVE_SVG_POINTER_EVENTS_NONE) return false;
    switch (mode) {
    case NATIVE_SVG_POINTER_EVENTS_VISIBLE_PAINTED:
        return visible && ((fill_painted && hit->fill) || (stroke_painted && hit->stroke));
    case NATIVE_SVG_POINTER_EVENTS_VISIBLE_FILL:
        return visible && hit->fill;
    case NATIVE_SVG_POINTER_EVENTS_VISIBLE_STROKE:
        return visible && hit->stroke;
    case NATIVE_SVG_POINTER_EVENTS_VISIBLE:
        return visible && (hit->fill || hit->stroke);
    case NATIVE_SVG_POINTER_EVENTS_PAINTED:
        return (fill_painted && hit->fill) || (stroke_painted && hit->stroke);
    case NATIVE_SVG_POINTER_EVENTS_FILL:
        return hit->fill;
    case NATIVE_SVG_POINTER_EVENTS_STROKE:
        return hit->stroke;
    case NATIVE_SVG_POINTER_EVENTS_ALL:
        return hit->fill || hit->stroke;
    case NATIVE_SVG_POINTER_EVENTS_BOUNDING_BOX:
        return hit->bounding_box;
    case NATIVE_SVG_POINTER_EVENTS_NONE:
        return false;
    }
    return false;
}

static bool dom_svg_element_skips_hit_test(DomElement* elem) {
    if (!elem || !elem->tag_name || !dom_svg_element_is_eligible(elem)) return true;
    const char* tag = elem->local_name();
    if (str_icmp_cstr(tag, "defs") == 0 || str_icmp_cstr(tag, "clipPath") == 0 ||
        str_icmp_cstr(tag, "mask") == 0 || str_icmp_cstr(tag, "marker") == 0 ||
        str_icmp_cstr(tag, "pattern") == 0 || str_icmp_cstr(tag, "linearGradient") == 0 ||
        str_icmp_cstr(tag, "radialGradient") == 0 || str_icmp_cstr(tag, "title") == 0 ||
        str_icmp_cstr(tag, "desc") == 0 || str_icmp_cstr(tag, "metadata") == 0) {
        return true;
    }
    char display_buffer[64] = {};
    const char* display = dom_svg_presentation_value(elem, "display", false,
        display_buffer, sizeof(display_buffer));
    // visibility and pointer-events are inherited, but descendants may
    // override either one. Only display:none removes the entire subtree.
    return display && str_icmp_cstr(display, "none") == 0;
}

static bool dom_svg_point_is_within_viewports(DomElement* elem, float x, float y) {
    for (DomNode* node = (DomNode*)elem; node && node->is_element(); node = node->parent) {
        DomElement* current = node->as_element();
        if (!dom_svg_tag_is(current, "svg")) continue;
        char overflow_buffer[64] = {};
        const char* overflow = dom_svg_presentation_value(current, "overflow", false,
            overflow_buffer, sizeof(overflow_buffer));
        if (overflow && str_icmp_cstr(overflow, "visible") == 0) continue;
        float local_x = 0.0f;
        float local_y = 0.0f;
        RdtMatrix ctm = dom_svg_ctm(current, true);
        if (!rdt_matrix_unproject_affine_point(&ctm, x, y, &local_x, &local_y)) {
            return false;
        }
        float left = 0.0f;
        float top = 0.0f;
        float right = 0.0f;
        float bottom = 0.0f;
        if (!dom_svg_viewport_local_bounds(current, &left, &top, &right, &bottom) ||
            local_x < left || local_x > right || local_y < top || local_y > bottom) {
            return false;
        }
    }
    return true;
}

static bool dom_svg_point_is_within_clips(DomElement* elem, float x, float y) {
    for (DomNode* node = elem; node && node->is_element(); node = node->parent) {
        DomElement* current = node->as_element();
        if (!dom_element_is_svg(current)) break;
        if (!dom_svg_clip_contains(current, nullptr, x, y)) return false;
    }
    return true;
}

static bool dom_svg_element_contains_viewport_point(DomElement* elem,
                                                       float x, float y) {
    if (!elem || dom_svg_element_skips_hit_test(elem) ||
        !dom_svg_point_is_within_viewports(elem, x, y) ||
        !dom_svg_point_is_within_clips(elem, x, y)) return false;
    NativeSvgPointerEventsMode pointer_events = dom_svg_pointer_events_mode(elem);
    if (pointer_events == NATIVE_SVG_POINTER_EVENTS_NONE) return false;
    NativeSvgShapeHit hit = {};
    bool fill_painted = false;
    bool stroke_painted = false;
    if (dom_svg_is_basic_shape(elem) || dom_svg_is_text(elem)) {
        fill_painted = dom_svg_paint_is_present(elem, "fill", true);
        stroke_painted = dom_svg_paint_is_present(elem, "stroke", false);
        bool check_stroke = pointer_events != NATIVE_SVG_POINTER_EVENTS_FILL &&
            pointer_events != NATIVE_SVG_POINTER_EVENTS_VISIBLE_FILL &&
            pointer_events != NATIVE_SVG_POINTER_EVENTS_BOUNDING_BOX &&
            (stroke_painted || (pointer_events != NATIVE_SVG_POINTER_EVENTS_PAINTED &&
                pointer_events != NATIVE_SVG_POINTER_EVENTS_VISIBLE_PAINTED));
        hit = dom_svg_basic_shape_hit_viewport_point(elem, x, y, check_stroke);
    } else if (dom_svg_tag_is(elem, "use")) {
        hit = dom_svg_use_hit_viewport_point(elem, x, y);
        fill_painted = hit.fill_painted;
        stroke_painted = hit.stroke_painted;
    } else if (dom_svg_tag_is(elem, "g") || dom_svg_tag_is(elem, "a") ||
               dom_svg_tag_is(elem, "switch")) {
        // Container bounds are only a union for getBBox(); blank union gaps
        // belong to no painted child and must not become a drag target.
        return false;
    } else {
        hit = dom_svg_bounds_hit_viewport_point(elem, x, y);
        if (dom_svg_tag_is(elem, "image") ||
            dom_svg_tag_is(elem, "foreignObject") || dom_svg_tag_is(elem, "svg")) {
            fill_painted = true;
        } else {
            fill_painted = dom_svg_paint_is_present(elem, "fill", true);
            stroke_painted = dom_svg_paint_is_present(elem, "stroke", false);
        }
    }
    return dom_svg_pointer_events_selects_geometry(pointer_events,
        dom_svg_element_is_visible_for_pointer_events(elem), fill_painted,
        stroke_painted, &hit);
}

static DomElement* dom_svg_element_from_point_walk(DomElement* elem,
                                                       float x, float y) {
    DomSvgHitGeometryScope geometry(elem);
    SvgAnimationSourceScope animation_sources(elem ? elem->doc : nullptr);
    if (!elem || dom_svg_element_skips_hit_test(elem) ||
        !dom_svg_point_is_within_viewports(elem, x, y)) return nullptr;
    if (dom_svg_tag_is(elem, "foreignObject")) {
        if (!dom_svg_point_is_within_clips(elem, x, y)) return nullptr;
        float local_x, local_y;
        if (!dom_svg_foreign_object_local_point(elem, x, y, &local_x, &local_y)) return nullptr;
        // HTML targeting uses the existing CSS paint order, controls and overflow walker.
        DomElement* hit = (DomElement*)radiant_subtree_element_from_point(elem, local_x, local_y);
        return hit ? hit : (dom_svg_element_contains_viewport_point(elem, x, y) ? elem : nullptr);
    }
    for (DomNode* child = elem->last_child; child; child = child->prev_sibling) {
        if (!child->is_element()) continue;
        if (dom_svg_tag_is(elem, "switch") && child != dom_svg_switch_selected_child(elem)) continue;
        DomElement* child_elem = child->as_element();
        DomElement* hit = dom_svg_element_from_point_walk(child_elem, x, y);
        if (hit) return hit;
    }
    return dom_svg_element_contains_viewport_point(elem, x, y) ? elem : nullptr;
}

extern "C" void* dom_svg_element_from_point(void* element, float x, float y) {
    return dom_svg_element_from_point_walk((DomElement*)element, x, y);
}
