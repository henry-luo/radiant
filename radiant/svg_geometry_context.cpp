#include "svg_geometry_context.hpp"
#include "layout.hpp"
#include "layout_host.hpp"
#include "event.hpp"
#include "svg_animation.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lib/hashmap_typed.hpp"
#include "lib/str.h"
#include <math.h>
#include <string.h>

void dom_svg_apply_css_viewport_size(DomElement* elem, float* width, float* height) {
    if (elem->parent && elem->parent->is_element() && dom_element_is_svg(elem->parent->as_element())) return;
    // SVG descendants have no CSS box; the outer viewport uses committed layout.
    LayoutContentBox content = layout_content_box((ViewBlock*)elem);
    if (content.width > 0.0f) *width = content.width;
    if (content.height > 0.0f) *height = content.height;
}

thread_local const SvgDomStyleScope* g_dom_svg_style_scope = nullptr;

// Cache entries borrow their document and expire before event handlers run.
using DomSvgHitGeometryMap = TypedHashMap<DomSvgHitGeometry,
    HashMapPointerMemberKeyOps<DomSvgHitGeometry, &DomSvgHitGeometry::element>>;
static thread_local DomSvgHitGeometryScope* g_dom_svg_hit_geometry = nullptr;

DomSvgHitGeometryScope::DomSvgHitGeometryScope(DomElement* element) :
    document(element ? element->doc : nullptr), entries(nullptr),
    previous(g_dom_svg_hit_geometry), owner(document && (!previous || previous->document != document)) {
    if (owner) {
        entries = DomSvgHitGeometryMap::create(64);
        g_dom_svg_hit_geometry = this;
    }
}

DomSvgHitGeometryScope::~DomSvgHitGeometryScope() {
    if (owner) {
        g_dom_svg_hit_geometry = previous;
        DomSvgHitGeometryMap::destroy(entries);
    }
}

DomSvgHitGeometry* dom_svg_hit_geometry(DomElement* element, bool create) {
    auto* scope = g_dom_svg_hit_geometry;
    // A use instance supplies a different font/viewport ancestry for the same source.
    if (!element || !scope || !scope->entries || scope->document != element->doc ||
        g_dom_svg_style_scope || svg_animation_is_sampling(element->doc)) return nullptr;
    DomSvgHitGeometry key = {}; key.element = element;
    DomSvgHitGeometry* entry = DomSvgHitGeometryMap::get(scope->entries, key);
    if (!entry && create) {
        DomSvgHitGeometryMap::set(scope->entries, key);
        entry = DomSvgHitGeometryMap::get(scope->entries, key);
    }
    return entry;
}

SvgLengthContext dom_svg_length_context(DomElement* elem) {
    SvgLengthContext lengths = {0.0f, 0.0f, 16.0f, 8.0f};
    if (!elem) return lengths;
    DomSvgHitGeometry* cached = dom_svg_hit_geometry(elem);
    if (cached && cached->has_lengths) return cached->lengths;
    DomElement* chain[64];
    int count = 0;
    for (DomNode* node = elem; node && count < 64; node = svg_dom_style_parent(node, g_dom_svg_style_scope)) {
        if (node->is_element()) chain[count++] = node->as_element();
    }
    UiContext* ui = elem->doc ? (UiContext*)elem->doc->js.host_ui_context : nullptr;
    FontContext* fonts = ui ? ui->font_ctx : nullptr;
    char family[256] = "Arial";
    FontStyleDesc font = {}; font.family = family; font.size_px = 16.0f; font.weight = FONT_WEIGHT_NORMAL;
    bool in_svg = false;
    for (int index = count - 1; index >= 0; index--) {
        DomElement* current = chain[index];
        bool svg = dom_svg_tag_is(current, "svg");
        if (svg) in_svg = true;
        char buffer[64];
        if (in_svg) {
            const char* size = svg_get_dom_presentation_property(current, "font-size", false,
                buffer, sizeof(buffer), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
            if (size) lengths.font_size = svg_font_size_value(size, lengths.font_size, svg_font_x_height(fonts, &font));
            font.size_px = lengths.font_size;
        } else if (current->font && current->font->font_size > 0.0f) {
            lengths.font_size = current->font->font_size;
            font = font_style_desc_from_prop(current->font);
            str_copy(family, sizeof(family), font.family ? font.family : "Arial", strlen(font.family ? font.family : "Arial")); font.family = family;
        }
        if (in_svg) {
            char own_family[256];
            const char* value = svg_get_dom_presentation_property(current, "font-family", false, own_family, sizeof(own_family), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
            if (value) str_copy(family, sizeof(family), value, strlen(value));
            value = svg_get_dom_presentation_property(current, "font-weight", false, buffer, sizeof(buffer), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
            if (value) font.weight = (FontWeight)svg_font_weight_value(value, font.weight);
            value = svg_get_dom_presentation_property(current, "font-style", false, buffer, sizeof(buffer), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
            if (value) font.slant = strcmp(value, "italic") == 0 ? FONT_SLANT_ITALIC
                : strcmp(value, "oblique") == 0 ? FONT_SLANT_OBLIQUE : FONT_SLANT_NORMAL;
        }
        const SvgDomStyleScope* viewport_scope = g_dom_svg_style_scope;
        while (viewport_scope && viewport_scope->root != current) viewport_scope = viewport_scope->previous;
        if (viewport_scope && viewport_scope->has_viewport) {
            lengths.viewport_width = viewport_scope->viewport_width;
            lengths.viewport_height = viewport_scope->viewport_height;
            continue;
        }
        if (!svg || current == elem) continue;
        const char* viewbox = svg_animation_attribute(current, "viewBox");
        if (!viewbox) viewbox = svg_animation_attribute(current, "viewbox");
        SvgViewBox box = svg_parse_viewbox(viewbox);
        if (box.has_viewbox && box.width > 0.0f && box.height > 0.0f) {
            lengths.viewport_width = box.width; lengths.viewport_height = box.height;
        } else {
            float width = svg_resolve_length(svg_animation_attribute(current, "width"), &lengths, SVG_LENGTH_X,
                current->width > 0.0f ? current->width : lengths.viewport_width);
            float height = svg_resolve_length(svg_animation_attribute(current, "height"), &lengths, SVG_LENGTH_Y,
                current->height > 0.0f ? current->height : lengths.viewport_height);
            dom_svg_apply_css_viewport_size(current, &width, &height);
            lengths.viewport_width = width; lengths.viewport_height = height;
        }
    }
    lengths.x_height = svg_font_x_height(fonts, &font);
    cached = dom_svg_hit_geometry(elem, true);
    if (cached) { cached->lengths = lengths; cached->has_lengths = true; }
    return lengths;
}


extern "C" bool dom_svg_foreign_object_rectangle(void* element, float* x, float* y, float* width, float* height) {
    DomElement* elem = (DomElement*)element;
    if (!elem || !x || !y || !width || !height) return false;
    SvgLengthContext lengths = dom_svg_length_context(elem);
    static const char* const names[] = {"x", "y", "width", "height"};
    float* values[] = {x, y, width, height};
    Element* backing = dom_element_to_element(elem);
    for (unsigned index = 0; index < 4; index++) {
        char buffer[256] = {}; bool from_css = false;
        const char* value = svg_get_dom_presentation_property(elem, names[index], false,
            buffer, sizeof(buffer), &from_css, nullptr, nullptr, g_dom_svg_style_scope);
        double number = 0.0;
        Item attribute = backing ? Item{.item = backing->get_attr(names[index]).item} : ItemNull;
        *values[index] = !from_css && !svg_animation_value(elem, names[index], false) && item_try_to_double(attribute, &number) ? (float)number :
            svg_resolve_length(value, &lengths, index % 2 ? SVG_LENGTH_Y : SVG_LENGTH_X, 0.0f);
        if (!isfinite(*values[index])) return false;
    }
    return *width >= 0.0f && *height >= 0.0f;
}

extern "C" bool dom_svg_foreign_object_clips(void* element) {
    DomElement* elem = (DomElement*)element; if (!elem) return true;
    char buffer[64] = {}; bool from_css = false;
    const char* overflow = svg_get_dom_presentation_property(elem, "overflow", false,
        buffer, sizeof(buffer), &from_css, nullptr, nullptr, g_dom_svg_style_scope);
    // SVG's UA overflow:hidden applies unless the author supplies an overflow value.
    return (!from_css && !elem->has_attribute("overflow")) || !overflow || str_icmp_cstr(overflow, "visible") != 0;
}


static void dom_svg_bounds_include_point(SvgGeometryBounds* bounds, float x, float y) {
    if (!bounds) return;
    if (!bounds->valid) {
        bounds->left = bounds->right = x;
        bounds->top = bounds->bottom = y;
        bounds->valid = true;
        return;
    }
    bounds->left = LMB_MIN(bounds->left, x);
    bounds->top = LMB_MIN(bounds->top, y);
    bounds->right = LMB_MAX(bounds->right, x);
    bounds->bottom = LMB_MAX(bounds->bottom, y);
}

static void dom_svg_bounds_include_rect(SvgGeometryBounds* bounds, float x, float y,
                                            float width, float height) {
    if (!bounds) return;
    dom_svg_bounds_include_point(bounds, x, y);
    dom_svg_bounds_include_point(bounds, x + width, y + height);
}

static void dom_svg_bounds_include_bounds(SvgGeometryBounds* bounds,
                                              const SvgGeometryBounds* child) {
    if (!bounds || !child || !child->valid) return;
    dom_svg_bounds_include_point(bounds, child->left, child->top);
    dom_svg_bounds_include_point(bounds, child->right, child->bottom);
}

void dom_svg_bounds_apply_transform(SvgGeometryBounds* bounds,
                                               const RdtMatrix* transform) {
    if (!bounds || !bounds->valid || !transform) return;
    float left = 0.0f;
    float top = 0.0f;
    float right = 0.0f;
    float bottom = 0.0f;
    rdt_matrix_transform_rect_bounds(transform, bounds->left, bounds->top,
                                     bounds->right, bounds->bottom,
                                     &left, &top, &right, &bottom);
    bounds->left = left;
    bounds->top = top;
    bounds->right = right;
    bounds->bottom = bottom;
}

float dom_svg_attribute_number(DomElement* elem, const char* name, float fallback) {
    if (!elem || !name) return fallback;
    SvgLengthContext lengths = dom_svg_length_context(elem);
    return svg_resolve_length(svg_animation_attribute(elem, name), &lengths, dom_svg_length_axis(name), fallback);
}

RdtMatrix dom_svg_transform_from_element(DomElement* elem) {
    RdtMatrix transform = rdt_matrix_identity();
    if (!elem) return transform;
    DomSvgHitGeometry* cached = dom_svg_hit_geometry(elem);
    if (cached && cached->has_local) return cached->local;
    char value_buffer[512];
    bool from_css = false;
    const char* value = svg_get_dom_presentation_property(elem, "transform", false,
        value_buffer, sizeof(value_buffer), &from_css, nullptr, nullptr, g_dom_svg_style_scope);
    const char* names[] = {"translate", "rotate", "scale"};
    char individual_buffers[3][256];
    const char* individual[3] = {};
    bool has_individual = false;
    for (unsigned index = 0; index < 3; index++) {
        individual[index] = svg_get_dom_presentation_property(elem, names[index], false,
            individual_buffers[index], sizeof(individual_buffers[index]), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
        has_individual |= individual[index] && strcmp(individual[index], "none");
    }
    bool nested_viewport = dom_svg_tag_is(elem, "svg") &&
        elem->parent && elem->parent->is_element() && dom_element_is_svg(elem->parent->as_element());
    // individual transforms also depend on the SVG reference box when the list is absent.
    if ((!value || strcmp(value, "none") == 0) && !has_individual && !nested_viewport) return transform;
    SvgLengthContext lengths = dom_svg_length_context(elem);
    Bound box = {0.0f, 0.0f, lengths.viewport_width, lengths.viewport_height};
    for (DomNode* node = svg_dom_style_parent(elem, g_dom_svg_style_scope); node && node->is_element(); node = svg_dom_style_parent(node, g_dom_svg_style_scope)) {
        DomElement* parent = node->as_element();
        if (!dom_svg_tag_is(parent, "svg")) continue;
        const char* viewbox = svg_animation_attribute(parent, "viewBox");
        if (!viewbox) viewbox = svg_animation_attribute(parent, "viewbox");
        SvgViewBox parent_box = svg_parse_viewbox(viewbox);
        if (parent_box.has_viewbox) {
            box = {parent_box.min_x, parent_box.min_y, parent_box.min_x + parent_box.width,
                parent_box.min_y + parent_box.height};
        }
        break;
    }
    char box_buffer[64];
    const char* reference = svg_get_dom_presentation_property(elem, "transform-box", false,
        box_buffer, sizeof(box_buffer), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
    if (reference && (strcmp(reference, "fill-box") == 0 || strcmp(reference, "stroke-box") == 0)) {
        dom_svg_element_geometry_bounds(elem, &box.left, &box.top, &box.right, &box.bottom);
    }
    char origin_buffer[128];
    const char* origin = svg_get_dom_presentation_property(elem, "transform-origin", false,
        origin_buffer, sizeof(origin_buffer), nullptr, nullptr, nullptr, g_dom_svg_style_scope);
    transform = svg_resolve_local_transform(value, from_css, origin, &box, &lengths,
        has_individual ? individual : nullptr);
    if (nested_viewport) {
        RdtMatrix viewport_offset = rdt_matrix_translate(
            dom_svg_attribute_number(elem, "x", 0.0f), dom_svg_attribute_number(elem, "y", 0.0f));
        transform = rdt_matrix_multiply(&transform, &viewport_offset);
    }
    cached = dom_svg_hit_geometry(elem, true);
    if (cached) { cached->local = transform; cached->has_local = true; }
    return transform;
}


extern "C" bool dom_svg_is_text(void* element) {
    DomElement* elem = (DomElement*)element;
    return dom_svg_tag_is(elem, "text") || dom_svg_tag_is(elem, "tspan") || dom_svg_tag_is(elem, "textPath");
}

RdtPath* dom_svg_text_geometry_path(DomElement* elem) {
    SvgLengthContext lengths = dom_svg_length_context(elem);
    UiContext* ui = elem->doc ? (UiContext*)elem->doc->js.host_ui_context : nullptr;
    return svg_text_geometry_path(elem, &lengths, ui ? ui->font_ctx : nullptr);
}

SvgGeometryBounds dom_svg_bounds_for_element(DomElement* elem, int depth) {
    SvgAnimationSourceScope animation_sources(elem ? elem->doc : nullptr);
    SvgGeometryBounds bounds = {};
    if (!dom_element_is_svg(elem) || depth >= SVG_USE_DEPTH_MAX) return bounds;
    const char* tag = elem->local_name();
    if (dom_svg_is_basic_shape(elem)) {
        RdtPath* path = dom_svg_basic_shape_path(elem);
        if (path) {
            bounds.valid = rdt_path_get_bounds(path, &bounds.left, &bounds.top, &bounds.right, &bounds.bottom);
            rdt_path_free(path);
        }
    } else if (str_icmp_cstr(tag, "foreignObject") == 0) {
        float x, y, width, height;
        if (dom_svg_foreign_object_rectangle(elem, &x, &y, &width, &height))
            dom_svg_bounds_include_rect(&bounds, x, y, width, height);
    } else if (str_icmp_cstr(tag, "image") == 0) {
        dom_svg_bounds_include_rect(&bounds, dom_svg_attribute_number(elem, "x", 0.0f),
            dom_svg_attribute_number(elem, "y", 0.0f), dom_svg_attribute_number(elem, "width", 0.0f),
            dom_svg_attribute_number(elem, "height", 0.0f));
    } else if (dom_svg_is_text(elem)) {
        RdtPath* path = dom_svg_text_geometry_path(elem);
        if (path) {
            bounds.valid = rdt_path_get_bounds(path, &bounds.left, &bounds.top, &bounds.right, &bounds.bottom);
            rdt_path_free(path);
        }
    } else if (str_icmp_cstr(tag, "use") == 0) {
        DomElement* reference = dom_svg_use_reference(elem);
        SvgAnimationSourceScope animation_scope(elem->doc, reference, elem);
        bounds = dom_svg_bounds_for_element(reference, depth + 1);
        RdtMatrix reference_transform = reference ? dom_svg_transform_from_element(reference) : rdt_matrix_identity();
        dom_svg_bounds_apply_transform(&bounds, &reference_transform);
        RdtMatrix offset = rdt_matrix_translate(dom_svg_attribute_number(elem, "x", 0.0f),
            dom_svg_attribute_number(elem, "y", 0.0f));
        dom_svg_bounds_apply_transform(&bounds, &offset);
    } else if (str_icmp_cstr(tag, "g") == 0 || str_icmp_cstr(tag, "svg") == 0 ||
               str_icmp_cstr(tag, "a") == 0 || str_icmp_cstr(tag, "switch") == 0 ||
               str_icmp_cstr(tag, "symbol") == 0 || str_icmp_cstr(tag, "clipPath") == 0) {
        for (DomNode* child = elem->first_child; child; child = child->next_sibling) {
            if (!child->is_element()) continue;
            if (str_icmp_cstr(tag, "switch") == 0 && child != dom_svg_switch_selected_child(elem)) continue;
            DomElement* child_elem = child->as_element();
            SvgGeometryBounds child_bounds = dom_svg_bounds_for_element(child_elem, depth + 1);
            RdtMatrix child_transform = dom_svg_transform_from_element(child_elem);
            dom_svg_bounds_apply_transform(&child_bounds, &child_transform);
            dom_svg_bounds_include_bounds(&bounds, &child_bounds);
        }
    }
    return bounds;
}

bool dom_svg_element_geometry_bounds(void* element, float* left, float* top, float* right, float* bottom) {
    if (!element || !left || !top || !right || !bottom) return false;
    SvgAnimationSourceScope animation_sources(((DomElement*)element)->doc);
    SvgGeometryBounds bounds = dom_svg_bounds_for_element((DomElement*)element);
    if (!bounds.valid) return false;
    *left = bounds.left; *top = bounds.top; *right = bounds.right; *bottom = bounds.bottom;
    return true;
}


bool dom_svg_is_basic_shape(DomElement* elem) {
    if (!dom_element_is_svg(elem)) return false;
    const char* tag = elem->local_name();
    return str_icmp_cstr(tag, "rect") == 0 || str_icmp_cstr(tag, "circle") == 0 ||
        str_icmp_cstr(tag, "ellipse") == 0 || str_icmp_cstr(tag, "line") == 0 ||
        str_icmp_cstr(tag, "polyline") == 0 || str_icmp_cstr(tag, "polygon") == 0 ||
        str_icmp_cstr(tag, "path") == 0;
}

RdtPath* dom_svg_basic_shape_path(DomElement* elem) {
    if (!dom_svg_is_basic_shape(elem)) return nullptr;
    if (str_icmp_cstr(elem->local_name(), "path") == 0) return svg_parse_path_d(svg_animation_attribute(elem, "d"));
    RdtPath* path = rdt_path_new();
    SvgLengthContext lengths = dom_svg_length_context(elem);
    if (!path || !svg_append_basic_shape_path(elem, path, &lengths)) {
        if (path) rdt_path_free(path);
        return nullptr;
    }
    return path;
}


DomElement* dom_svg_use_reference(DomElement* elem) {
    if (!elem || !elem->doc || !dom_svg_tag_is(elem, "use")) return nullptr;
    const char* href = elem->get_attribute("href");
    if (!href) href = dom_element_attribute_value_ns(elem, "http://www.w3.org/1999/xlink", "href");
    if (!href || !*href) return nullptr;
    if (href[0] != '#') return svg_animation_use_source(elem);
    if (!href[1]) return nullptr;
    return dom_find_element_by_id(elem->doc->root, href + 1);
}


bool dom_svg_parse_viewbox(const char* value, float* min_x, float* min_y,
    float* width, float* height) {
    if (!value || !min_x || !min_y || !width || !height) return false;
    SvgViewBox box = svg_parse_viewbox(value);
    if (!box.has_viewbox || box.width <= 0.0f || box.height <= 0.0f) return false;
    *min_x = box.min_x; *min_y = box.min_y; *width = box.width; *height = box.height;
    return true;
}

static RdtMatrix dom_svg_viewbox_transform(DomElement* elem) {
    RdtMatrix transform = rdt_matrix_identity();
    if (!elem) return transform;
    float min_x = 0.0f;
    float min_y = 0.0f;
    float viewbox_width = 0.0f;
    float viewbox_height = 0.0f;
    const char* viewbox = svg_animation_attribute(elem, "viewBox");
    if (!viewbox) viewbox = svg_animation_attribute(elem, "viewbox");
    if (!dom_svg_parse_viewbox(viewbox, &min_x, &min_y, &viewbox_width, &viewbox_height)) {
        return transform;
    }
    float viewport_width = dom_svg_attribute_number(elem, "width", 0.0f);
    float viewport_height = dom_svg_attribute_number(elem, "height", 0.0f);
    dom_svg_apply_css_viewport_size(elem, &viewport_width, &viewport_height);
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) {
        // hit-testing already has committed CSS geometry; flushing through the
        // generic DOM path can rebuild this element before the second axis.
        if (viewport_width <= 0.0f) viewport_width = elem->width;
        if (viewport_height <= 0.0f) viewport_height = elem->height;
    }
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) {
        if (viewport_width <= 0.0f) {
            viewport_width = radiant_layout_geometry_fallback_dimension(elem, true);
        }
        if (viewport_height <= 0.0f) {
            viewport_height = radiant_layout_geometry_fallback_dimension(elem, false);
        }
    }
    if (viewport_width <= 0.0f || viewport_height <= 0.0f) return transform;
    SvgViewBox box = {min_x, min_y, viewbox_width, viewbox_height, true};
    return svg_viewbox_transform(&box, viewport_width, viewport_height,
        svg_animation_attribute(elem, "preserveAspectRatio"));
}

RdtMatrix dom_svg_ctm(DomElement* elem, bool screen_space) {
    RdtMatrix matrix = rdt_matrix_identity();
    if (!elem) return matrix;
    DomSvgHitGeometry* cached = dom_svg_hit_geometry(elem);
    if (cached && (screen_space ? cached->has_screen : cached->has_viewport))
        return screen_space ? cached->screen : cached->viewport;
    DomElement* chain[64];
    int count = 0;
    DomElement* outermost_svg = nullptr;
    for (DomNode* current = (DomNode*)elem; current && count < 64;
         current = current->parent) {
        if (!current->is_element()) continue;
        DomElement* candidate = current->as_element();
        // an HTML integration point starts a new CSS-positioned SVG viewport.
        if (!dom_element_is_svg(candidate)) break;
        chain[count++] = candidate;
        if (candidate && dom_svg_tag_is(candidate, "svg")) {
            outermost_svg = candidate;
        }
    }
    bool in_svg = false;
    for (int i = count - 1; i >= 0; i--) {
        DomElement* current = chain[i];
        if (!current || !current->tag_name) continue;
        bool svg = dom_svg_tag_is(current, "svg");
        if (svg) in_svg = true;
        if (in_svg) {
            RdtMatrix local = dom_svg_transform_from_element(current);
            matrix = rdt_matrix_multiply(&matrix, &local);
        }
        if (svg) {
            RdtMatrix viewbox = dom_svg_viewbox_transform(current);
            matrix = rdt_matrix_multiply(&matrix, &viewbox);
        }
    }
    if (screen_space && outermost_svg) {
        RdtMatrix layout_transform;
        if (!view_get_foreign_object_matrix(static_cast<View*>(outermost_svg), &layout_transform, false)) {
            RdtLogicalPoint origin = {};
            view_get_layout_position(static_cast<View*>(outermost_svg), nullptr, &origin.x, &origin.y);
            layout_transform = rdt_matrix_translate(origin.x, origin.y);
        }
        matrix = rdt_matrix_multiply(&layout_transform, &matrix);
    }
    cached = dom_svg_hit_geometry(elem, true);
    if (cached) {
        if (screen_space) { cached->screen = matrix; cached->has_screen = true; }
        else { cached->viewport = matrix; cached->has_viewport = true; }
    }
    return matrix;
}

extern "C" bool dom_svg_foreign_object_client_transform(void* element, float matrix[6]) {
    DomElement* elem = (DomElement*)element;
    float x, y, width, height;
    if (!elem || !matrix || !dom_svg_foreign_object_rectangle(elem, &x, &y, &width, &height)) return false;
    RdtMatrix ctm = dom_svg_ctm(elem, true), offset = rdt_matrix_translate(x, y);
    ctm = rdt_matrix_multiply(&ctm, &offset);
    matrix[0] = ctm.e11; matrix[1] = ctm.e12; matrix[2] = ctm.e13;
    matrix[3] = ctm.e21; matrix[4] = ctm.e22; matrix[5] = ctm.e23;
    return true;
}

extern "C" bool dom_svg_foreign_object_local_point(void* element, float x, float y,
                                                   float* local_x, float* local_y) {
    float values[6];
    if (!local_x || !local_y || !dom_svg_foreign_object_client_transform(element, values)) return false;
    RdtMatrix frame = {values[0], values[1], values[2], values[3], values[4], values[5], 0, 0, 1}, inverse;
    if (!rdt_matrix_invert_affine(&frame, &inverse)) return false;
    rdt_matrix_transform_point(&inverse, x, y, local_x, local_y);
    float rx, ry, width, height;
    return dom_svg_foreign_object_rectangle(element, &rx, &ry, &width, &height) && width > 0 && height > 0 &&
        (!dom_svg_foreign_object_clips(element) || (*local_x >= 0 && *local_y >= 0 && *local_x < width && *local_y < height));
}

extern "C" bool dom_svg_element_client_bounds(void* dom_elem, float* x, float* y,
                                              float* width, float* height) {
    DomElement* elem = (DomElement*)dom_elem;
    if (!elem || !elem->tag_name || !dom_element_is_svg(elem)) return false;
    // The outer <svg> is a CSS box, and so is HTML inside <foreignObject>
    // (reached before any <svg> on the way up); only SVG content below an
    // <svg> lacks one.
    bool inside_svg = false;
    for (DomNode* up = elem->parent; up; up = up->parent) {
        if (!up->is_element()) continue;
        DomElement* ancestor = up->as_element();
        if (!ancestor || !ancestor->tag_name) continue;
        if (dom_svg_tag_is(ancestor, "foreignObject")) return false;
        if (dom_svg_tag_is(ancestor, "svg")) { inside_svg = true; break; }
    }
    if (!inside_svg) return false;
    SvgGeometryBounds bounds = dom_svg_bounds_for_element(elem);
    if (!bounds.valid) return false;
    RdtMatrix screen = dom_svg_ctm(elem, true);
    dom_svg_bounds_apply_transform(&bounds, &screen);
    if (x) *x = bounds.left;
    if (y) *y = bounds.top;
    if (width) *width = bounds.right - bounds.left;
    if (height) *height = bounds.bottom - bounds.top;
    return true;
}

