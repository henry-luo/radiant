#include "render.hpp"
#include "layout.hpp"

#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/css_style.hpp"
#include "tagged.hpp"
#include "lib/log.h"
#include "lib/str.h"

#include <stddef.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>

static RdtPath* render_clip_create_rounded_rect_path(float x, float y, float w, float h,
                                                     float r_tl, float r_tr,
                                                     float r_br, float r_bl) {
    float max_rx = w * 0.5f, max_ry = h * 0.5f;
    if (r_tl > max_rx) r_tl = max_rx; if (r_tl > max_ry) r_tl = max_ry;
    if (r_tr > max_rx) r_tr = max_rx; if (r_tr > max_ry) r_tr = max_ry;
    if (r_br > max_rx) r_br = max_rx; if (r_br > max_ry) r_br = max_ry;
    if (r_bl > max_rx) r_bl = max_rx; if (r_bl > max_ry) r_bl = max_ry;
    Corner radius = render_path_uniform_corner(r_tl, r_tr, r_br, r_bl);
    Rect rect = {x, y, w, h};
    return render_path_create_rounded_rect(rect, &radius);
}

static RdtPath* render_clip_create_shape_path(ClipShape* shape) {
    if (!shape) return nullptr;
    RdtPath* p = rdt_path_new();
    switch (shape->type) {
        case CLIP_SHAPE_ROUNDED_RECT: {
            auto& rr = shape->rounded_rect;
            rdt_path_free(p);
            return render_clip_create_rounded_rect_path(rr.x, rr.y, rr.w, rr.h,
                rr.r_tl, rr.r_tr, rr.r_br, rr.r_bl);
        }
        case CLIP_SHAPE_CIRCLE:
            rdt_path_add_circle(p, shape->circle.cx, shape->circle.cy,
                                shape->circle.r, shape->circle.r);
            break;
        case CLIP_SHAPE_ELLIPSE:
            rdt_path_add_circle(p, shape->ellipse.cx, shape->ellipse.cy,
                                shape->ellipse.rx, shape->ellipse.ry);
            break;
        case CLIP_SHAPE_INSET:
            rdt_path_add_rect(p, shape->inset.x, shape->inset.y,
                              shape->inset.w, shape->inset.h, 0, 0);
            break;
        case CLIP_SHAPE_POLYGON:
            if (shape->polygon.count >= 3) {
                rdt_path_move_to(p, shape->polygon.vx[0], shape->polygon.vy[0]);
                for (int i = 1; i < shape->polygon.count; i++) {
                    rdt_path_line_to(p, shape->polygon.vx[i], shape->polygon.vy[i]);
                }
                rdt_path_close(p);
            }
            break;
        default:
            break;
    }
    return p;
}

bool compositor_clip_from_shape(const ClipShape* shape, CompositorClip* out) {
    if (!shape || !out || shape->transformed) return false;
    *out = {};
    out->type = shape->type;
    switch (shape->type) {
        case CLIP_SHAPE_ROUNDED_RECT: {
            const auto& r = shape->rounded_rect;
            const float values[8] = {r.x, r.y, r.w, r.h, r.r_tl, r.r_tr, r.r_br, r.r_bl};
            memcpy(out->values, values, sizeof(values));
            return true;
        }
        case CLIP_SHAPE_CIRCLE: {
            const float values[3] = {shape->circle.cx, shape->circle.cy, shape->circle.r};
            memcpy(out->values, values, sizeof(values));
            return true;
        }
        case CLIP_SHAPE_ELLIPSE: {
            const auto& e = shape->ellipse;
            const float values[4] = {e.cx, e.cy, e.rx, e.ry};
            memcpy(out->values, values, sizeof(values));
            return true;
        }
        case CLIP_SHAPE_INSET: {
            const auto& r = shape->inset;
            const float values[6] = {r.x, r.y, r.w, r.h, r.rx, r.ry};
            memcpy(out->values, values, sizeof(values));
            return true;
        }
        default:
            return false;
    }
}

ImageSurface* compositor_clip_coverage(const CompositorClip* clips, unsigned count, Bound region) {
    int width = (int)(region.right - region.left), height = (int)(region.bottom - region.top); // INT_CAST_OK: whole-pixel region
    ImageSurface* mask = width > 0 && height > 0 ? image_surface_create(width, height) : nullptr;
    if (!mask) return nullptr;
    for (int y = 0; y < height; y++) memset((uint8_t*)mask->pixels + (size_t)y * mask->pitch, 0, (size_t)width * 4);
    RdtVector vec = {};
    rdt_vector_init(&vec, (uint32_t*)mask->pixels, width, height, mask->pitch / 4);
    if (!vec.impl) { image_surface_destroy(mask); return nullptr; }
    // a tile offset keeps the paths in the surface coordinates a replay clips by
    rdt_vector_set_tile_offset_x(&vec, region.left);
    rdt_vector_set_tile_offset_y(&vec, region.top);
    rdt_vector_begin_batch(&vec);
    unsigned pushed = 0;
    bool complete = true;
    for (unsigned i = 0; complete && i < count; i++) {
        const float* v = clips[i].values;
        ClipShape shape = {};
        shape.type = clips[i].type;
        switch (shape.type) {
            case CLIP_SHAPE_ROUNDED_RECT: shape.rounded_rect = {v[0], v[1], v[2], v[3], v[4], v[5], v[6], v[7]}; break;
            case CLIP_SHAPE_CIRCLE: shape.circle = {v[0], v[1], v[2]}; break;
            case CLIP_SHAPE_ELLIPSE: shape.ellipse = {v[0], v[1], v[2], v[3]}; break;
            case CLIP_SHAPE_INSET: shape.inset = {v[0], v[1], v[2], v[3], v[4], v[5]}; break;
            default: complete = false; continue;
        }
        RdtPath* path = render_clip_create_shape_path(&shape);
        complete = path != nullptr;
        if (!path) break;
        rdt_push_clip(&vec, path, nullptr);
        rdt_path_free(path);
        pushed++;
    }
    // full coverage through every clip leaves each pixel's clip coverage in its alpha
    Color white;
    white.c = 0xFFFFFFFF;
    if (complete) rdt_fill_rect(&vec, region.left, region.top, (float)width, (float)height, white);
    rdt_vector_flush_batch(&vec);
    while (pushed--) rdt_pop_clip(&vec);
    rdt_vector_destroy(&vec);
    if (complete) return mask;
    image_surface_destroy(mask);
    return nullptr;
}

const char* render_clip_css_value(ViewElement* view) {
    if (!view) return nullptr;
    DomElement* element = lam::dom_require_element(lam::view_dom_node(view));
    CssDeclaration* declaration = dom_element_get_specified_value(element, CSS_PROPERTY_CLIP_PATH);
    return declaration && declaration->value_text_len > 0 ? declaration->value_text : nullptr;
}

// Owned clip shapes (and polygon vertices) live in the clip scope's scratch
// scope, which render_clip_pop_scope ends.
static ClipShape* render_clip_alloc_shape(ScratchArena* scratch, ScratchMark* mem) {
    return (ClipShape*)scratch_scope_calloc(scratch, mem, sizeof(ClipShape));
}

static ClipShape* render_clip_clone_shape(ScratchArena* scratch, ScratchMark* mem,
                                          const ClipShape* shape) {
    if (!scratch || !shape) return nullptr;
    ClipShape* copy = render_clip_alloc_shape(scratch, mem);
    if (!copy) return nullptr;
    *copy = *shape;
    return copy;
}

static bool render_clip_parse_polygon_len(const char*& s, float ref, float* out_value) {
    while (isspace((unsigned char)*s) || *s == ',') s++;
    const char* start = s;
    char* end = nullptr;
    float val = strtof(s, &end);
    if (end == start) {
        return false;
    }
    s = end;
    while (isspace((unsigned char)*s)) s++;
    if (*s == '%') {
        s++;
        *out_value = val / 100.0f * ref;
        return true;
    }
    if (s[0] == 'p' && s[1] == 'x') {
        s += 2;
        *out_value = val;
        return true;
    }
    if (isalpha((unsigned char)*s) || *s == '(') {
        return false;
    }
    *out_value = val;
    return true;
}

static bool render_clip_take_word(const char*& s, const char* word) {
    const char* start = s;
    while (isspace((unsigned char)*start)) start++;
    size_t length = strlen(word);
    if (strncmp(start, word, length) != 0 ||
        isalnum((unsigned char)start[length]) || start[length] == '-') return false;
    s = start + length;
    return true;
}

static void render_clip_parse_rule(const char*& s, RdtFillRule* rule) {
    *rule = render_clip_take_word(s, "evenodd") ? RDT_FILL_EVEN_ODD : RDT_FILL_WINDING;
    if (*rule == RDT_FILL_WINDING) render_clip_take_word(s, "nonzero");
}

static RdtPath* render_clip_parse_shape_function(const char* value,
    float width, float height, float abs_x, float abs_y, RdtFillRule* rule) {
    if (!value || strncmp(value, "shape(", 6) != 0) return nullptr;
    const char* s = value + 6;
    render_clip_parse_rule(s, rule);
    float x, y, start_x, start_y;
    if (!render_clip_take_word(s, "from") ||
        !render_clip_parse_polygon_len(s, width, &x) ||
        !render_clip_parse_polygon_len(s, height, &y)) return nullptr;
    start_x = x; start_y = y;
    RdtPath* path = rdt_path_new();
    if (!path) return nullptr;
    rdt_path_move_to(path, abs_x + x, abs_y + y);
    bool drawn = false;
    while (true) {
        while (isspace((unsigned char)*s)) s++;
        if (*s == ')') {
            if (drawn) return path;
            break;
        }
        if (*s++ != ',') break;
        if (render_clip_take_word(s, "close")) {
            rdt_path_close(path); x = start_x; y = start_y; drawn = true;
            continue;
        }
        bool move = render_clip_take_word(s, "move");
        bool horizontal = false, vertical = false;
        if (!move && !render_clip_take_word(s, "line")) {
            horizontal = render_clip_take_word(s, "hline");
            if (!horizontal) vertical = render_clip_take_word(s, "vline");
            if (!horizontal && !vertical) break;
        }
        bool relative = render_clip_take_word(s, "by");
        if (!relative && !render_clip_take_word(s, "to")) break;
        float next_x = 0.0f, next_y = 0.0f;
        if (!vertical && !render_clip_parse_polygon_len(s, width, &next_x)) break;
        if (!horizontal && !render_clip_parse_polygon_len(s, height, &next_y)) break;
        x = vertical ? x : next_x + (relative ? x : 0.0f);
        y = horizontal ? y : next_y + (relative ? y : 0.0f);
        if (move) {
            rdt_path_move_to(path, abs_x + x, abs_y + y); start_x = x; start_y = y;
        } else { rdt_path_line_to(path, abs_x + x, abs_y + y); drawn = true; }
    }
    rdt_path_free(path);
    return nullptr;
}

static bool render_clip_peek_number(const char* s) {
    s = str_skip_chars(s, ", \t\n\r\f\v");
    return *s == '-' || *s == '+' || *s == '.' || isdigit((unsigned char)*s);
}

static float render_clip_parse_number(const char** s) {
    *s = str_skip_chars(*s, ", \t\n\r\f\v");
    char* end = nullptr;
    float value = strtof(*s, &end);
    if (end == *s) {
        if (**s) (*s)++;
        return 0.0f;
    }
    *s = end;
    return value;
}

static RdtPath* render_clip_parse_path_function(const char* value,
    float width, float height, float abs_x, float abs_y, RdtFillRule* rule) {
    RdtPath* shape = render_clip_parse_shape_function(value, width, height, abs_x, abs_y, rule);
    if (shape) return shape;
    if (!value || strncmp(value, "path(", 5) != 0) return nullptr;
    const char* s = value + 5;
    render_clip_parse_rule(s, rule);
    while (isspace((unsigned char)*s) || *s == ',') s++;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s != '"' && *s != '\'') return nullptr;
    char quote = *s++;

    RdtPath* path = rdt_path_new();
    float cur_x = 0.0f, cur_y = 0.0f;
    float start_x = 0.0f, start_y = 0.0f;
    char last_cmd = 0;
    bool any_draw = false;

    while (*s && *s != quote) {
        s = str_skip_chars(s, ", \t\n\r\f\v");
        if (!*s || *s == quote) break;

        char cmd = *s;
        if (isalpha((unsigned char)cmd)) {
            s++;
            last_cmd = cmd;
        } else {
            if (!last_cmd || !render_clip_peek_number(s)) {
                rdt_path_free(path);
                return nullptr;
            }
            cmd = last_cmd;
            if (cmd == 'M') cmd = 'L';
            if (cmd == 'm') cmd = 'l';
        }

        bool relative = islower((unsigned char)cmd);
        cmd = (char)toupper((unsigned char)cmd);

        switch (cmd) {
            case 'M': {
                float x = render_clip_parse_number(&s);
                float y = render_clip_parse_number(&s);
                if (relative) { x += cur_x; y += cur_y; }
                cur_x = start_x = x;
                cur_y = start_y = y;
                rdt_path_move_to(path, abs_x + x, abs_y + y);
                while (render_clip_peek_number(s)) {
                    x = render_clip_parse_number(&s);
                    y = render_clip_parse_number(&s);
                    if (relative) { x += cur_x; y += cur_y; }
                    cur_x = x; cur_y = y;
                    rdt_path_line_to(path, abs_x + x, abs_y + y);
                    any_draw = true;
                }
                break;
            }
            case 'L': {
                while (render_clip_peek_number(s)) {
                    float x = render_clip_parse_number(&s);
                    float y = render_clip_parse_number(&s);
                    if (relative) { x += cur_x; y += cur_y; }
                    cur_x = x; cur_y = y;
                    rdt_path_line_to(path, abs_x + x, abs_y + y);
                    any_draw = true;
                }
                break;
            }
            case 'H': {
                while (render_clip_peek_number(s)) {
                    float x = render_clip_parse_number(&s);
                    if (relative) x += cur_x;
                    cur_x = x;
                    rdt_path_line_to(path, abs_x + cur_x, abs_y + cur_y);
                    any_draw = true;
                }
                break;
            }
            case 'V': {
                while (render_clip_peek_number(s)) {
                    float y = render_clip_parse_number(&s);
                    if (relative) y += cur_y;
                    cur_y = y;
                    rdt_path_line_to(path, abs_x + cur_x, abs_y + cur_y);
                    any_draw = true;
                }
                break;
            }
            case 'C': {
                while (render_clip_peek_number(s)) {
                    float x1 = render_clip_parse_number(&s);
                    float y1 = render_clip_parse_number(&s);
                    float x2 = render_clip_parse_number(&s);
                    float y2 = render_clip_parse_number(&s);
                    float x = render_clip_parse_number(&s);
                    float y = render_clip_parse_number(&s);
                    if (relative) {
                        x1 += cur_x; y1 += cur_y;
                        x2 += cur_x; y2 += cur_y;
                        x += cur_x; y += cur_y;
                    }
                    rdt_path_cubic_to(path, abs_x + x1, abs_y + y1,
                                      abs_x + x2, abs_y + y2,
                                      abs_x + x, abs_y + y);
                    cur_x = x; cur_y = y;
                    any_draw = true;
                }
                break;
            }
            case 'Z':
                rdt_path_close(path);
                cur_x = start_x;
                cur_y = start_y;
                any_draw = true;
                break;
            default:
                rdt_path_free(path);
                return nullptr;
        }
    }

    if (!any_draw) {
        rdt_path_free(path);
        return nullptr;
    }
    return path;
}

struct ClipComponents {
    const CssValue* value;
    int count() const { return !value ? 0 : value->type == CSS_VALUE_TYPE_LIST ? value->data.list.count : 1; }
    const CssValue* at(int index) const {
        if (index < 0 || index >= count()) return nullptr;
        return value->type == CSS_VALUE_TYPE_LIST ? value->data.list.values[index] : value;
    }
    const CssValue* tail(int index, CssValue* storage) const {
        if (!at(index)) return nullptr;
        if (value->type != CSS_VALUE_TYPE_LIST) return value;
        storage->type = CSS_VALUE_TYPE_LIST;
        storage->data.list = value->data.list;
        storage->data.list.values += index;
        storage->data.list.count -= index;
        return storage;
    }
};

static bool render_clip_word(const CssValue* value, const char* word) {
    const char* name = css_value_identifier_name(value);
    return name && str_icmp_cstr(name, word) == 0;
}

static bool render_clip_resolve_length(DomElement* owner, const CssValue* value,
        float reference, float unit_scale, float* result) {
    CssMathResult math = {};
    if (!(unit_scale > 0.0f) || !css_prop_compute_numeric_value(owner, value, &math, reference / unit_scale) ||
        (math.type != CSS_MATH_LENGTH && math.type != CSS_MATH_PERCENT &&
         math.type != CSS_MATH_LENGTH_PERCENT && !(math.type == CSS_MATH_NUMBER && math.value == 0.0)) ||
        !isfinite(math.value)) return false;
    *result = (float)math.value * unit_scale;
    return isfinite(*result);
}

static bool render_clip_resolve_center(DomElement* owner, ClipComponents components, int at,
        float width, float height, float unit_scale, float* x, float* y) {
    *x = width * 0.5f; *y = height * 0.5f;
    if (at < 0) return true;
    CssValue storage = {};
    CssPositionAxis axes[2] = {};
    if (!css_position_components(components.tail(at + 1, &storage), axes, false)) return false;
    for (int axis = 0; axis < 2; axis++) {
        float reference = axis ? height : width, offset = 0.0f;
        if (axes[axis].offset && !render_clip_resolve_length(owner, axes[axis].offset,
                reference, unit_scale, &offset)) return false;
        float position = axes[axis].origin == CSS_VALUE_CENTER ? reference * 0.5f + offset
            : axes[axis].origin == CSS_VALUE_RIGHT || axes[axis].origin == CSS_VALUE_BOTTOM
                ? reference - offset : offset;
        (axis ? *y : *x) = position;
    }
    return true;
}

static bool render_clip_resolve_radius(DomElement* owner, const CssValue* value,
        float reference, float nearest, float farthest, float unit_scale, float* radius) {
    if (!value || render_clip_word(value, "closest-side")) *radius = nearest;
    else if (render_clip_word(value, "farthest-side")) *radius = farthest;
    else if (!render_clip_resolve_length(owner, value, reference, unit_scale, radius)) return false;
    return *radius >= 0.0f;
}

static ClipShape* render_clip_parse_resolved_shape(ScratchArena* scratch, ScratchMark* mem,
        DomElement* owner, const CssValue* value, float elem_w, float elem_h,
        float abs_x, float abs_y, float unit_scale) {
    if (!value || value->type != CSS_VALUE_TYPE_FUNCTION || !value->data.function) return nullptr;
    const CssFunction* function = value->data.function;
    if (!function->name) return nullptr;
    ClipComponents components = {function->arg_count == 1 ? function->args[0] : nullptr};
    if (strcmp(function->name, "inset") == 0 && function->arg_count == 1) {
        int count = 0;
        while (count < components.count() && !render_clip_word(components.at(count), "round")) count++;
        if (count < 1 || count > 4) return nullptr;
        float edges[4] = {};
        for (int side = 0; side < 4; side++) {
            int index = side < count ? side : side == 3 && count > 1 ? 1 : 0;
            // expand syntax before resolving percentages against each physical axis.
            if (!render_clip_resolve_length(owner, components.at(index), side % 2 ? elem_w : elem_h,
                    unit_scale, &edges[side])) return nullptr;
        }
        float radius = 0.0f;
        if (count < components.count() && (!render_clip_resolve_length(owner, components.at(count + 1),
                elem_w, unit_scale, &radius) || radius < 0.0f)) return nullptr;
        ClipShape* shape = render_clip_alloc_shape(scratch, mem);
        if (!shape) return nullptr;
        float width = fmaxf(0.0f, elem_w - edges[1] - edges[3]);
        float height = fmaxf(0.0f, elem_h - edges[0] - edges[2]);
        if (radius > 0.0f) {
            shape->type = CLIP_SHAPE_ROUNDED_RECT;
            shape->rounded_rect = {abs_x + edges[3], abs_y + edges[0], width, height,
                radius, radius, radius, radius};
        } else {
            shape->type = CLIP_SHAPE_INSET;
            shape->inset = {abs_x + edges[3], abs_y + edges[0], width, height, 0, 0};
        }
        return shape;
    }
    bool circle = strcmp(function->name, "circle") == 0;
    if ((circle || strcmp(function->name, "ellipse") == 0) && function->arg_count <= 1) {
        int at = -1;
        for (int i = 0; i < components.count(); i++) if (render_clip_word(components.at(i), "at")) { at = i; break; }
        int radii = at >= 0 ? at : components.count();
        if (radii && radii != (circle ? 1 : 2)) return nullptr;
        float x, y;
        if (!render_clip_resolve_center(owner, components, at, elem_w, elem_h, unit_scale, &x, &y)) return nullptr;
        float near_x = fminf(fabsf(x), fabsf(elem_w - x)), near_y = fminf(fabsf(y), fabsf(elem_h - y));
        float far_x = fmaxf(fabsf(x), fabsf(elem_w - x)), far_y = fmaxf(fabsf(y), fabsf(elem_h - y));
        float rx, ry;
        if (!render_clip_resolve_radius(owner, radii ? components.at(0) : nullptr, circle ? hypotf(elem_w, elem_h) / sqrtf(2.0f) : elem_w,
                circle ? fminf(near_x, near_y) : near_x, circle ? fmaxf(far_x, far_y) : far_x,
                unit_scale, &rx)) return nullptr;
        if (circle) ry = rx;
        else if (!render_clip_resolve_radius(owner, radii ? components.at(1) : nullptr, elem_h, near_y, far_y, unit_scale, &ry)) return nullptr;
        ClipShape* shape = render_clip_alloc_shape(scratch, mem);
        if (!shape) return nullptr;
        if (circle) { shape->type = CLIP_SHAPE_CIRCLE; shape->circle = {abs_x + x, abs_y + y, rx}; }
        else { shape->type = CLIP_SHAPE_ELLIPSE; shape->ellipse = {abs_x + x, abs_y + y, rx, ry}; }
        return shape;
    }
    if (strcmp(function->name, "polygon") == 0 && function->arg_count >= 3) {
        int count = function->arg_count;
        float* x = (float*)scratch_scope_alloc(scratch, mem, (size_t)count * sizeof(float));
        float* y = (float*)scratch_scope_alloc(scratch, mem, (size_t)count * sizeof(float));
        if (!x || !y) return nullptr;
        for (int i = 0; i < count; i++) {
            ClipComponents point = {function->args[i]};
            if (point.count() != 2 || !render_clip_resolve_length(owner, point.at(0), elem_w, unit_scale, &x[i]) ||
                !render_clip_resolve_length(owner, point.at(1), elem_h, unit_scale, &y[i])) return nullptr;
            x[i] += abs_x; y[i] += abs_y;
        }
        ClipShape* shape = render_clip_alloc_shape(scratch, mem);
        if (!shape) return nullptr;
        shape->type = CLIP_SHAPE_POLYGON;
        shape->polygon = {lam::own_arr(x), lam::own_arr(y), count};
        return shape;
    }
    return nullptr;
}

static ClipShape* render_clip_parse_css_shape(ScratchArena* scratch, ScratchMark* mem,
        DomElement* owner, float elem_w, float elem_h, float abs_x, float abs_y, float scale) {
    CssDeclaration* declaration = owner ? dom_element_get_specified_value(owner, CSS_PROPERTY_CLIP_PATH) : nullptr;
    if (!scratch || !declaration) return nullptr;
    Pool* pool = pool_create();
    if (!pool) return nullptr;
    // keep substituted syntax temporary; only resolved coordinates enter the clip scope.
    const CssValue* value = css_resolve_element_var_value(pool, owner, declaration->value, CSS_PROPERTY_CLIP_PATH);
    ClipShape* shape = render_clip_parse_resolved_shape(scratch, mem, owner, value,
        elem_w, elem_h, abs_x, abs_y, scale * layout_effective_zoom(owner));
    pool_destroy(pool);
    return shape;
}

static bool render_clip_push_shape_scope(RasterRenderContext* rdcon, RenderClipScope* scope,
                                         ClipShape* shape, const RdtMatrix* transform) {
    if (!rdcon || !scope || !shape) {
        return false;
    }
    RdtPath* clip_path = render_clip_create_shape_path(shape);
    if (!clip_path) {
        return false;
    }
    // replay the original matrix; reconstructing it from the CPU inverse moves clip edges.
    rc_push_clip(rdcon, clip_path, transform);
    rdt_path_free(clip_path);

    scope->shape = lam::up(shape);
    scope->active = true;
    if (rdcon->clip_shape_depth < RDT_MAX_CLIP_SHAPES) {
        rdcon->clip_shapes[rdcon->clip_shape_depth++] = lam::up(shape);
        scope->pushed_shape = true;
    } else {
        log_warn("[RAD_CAP_RENDER_CLIP_SHAPES] dropping retained clip shape beyond depth %d",
                 RDT_MAX_CLIP_SHAPES);
    }
    return true;
}

// On success the clip scope takes over `mem`; on failure `mem` is ended here.
static bool render_clip_push_owned_shape(RasterRenderContext* rdcon,
                                         RenderClipScope* scope,
                                         ClipShape* shape, ScratchMark* mem,
                                         const RdtMatrix* transform = nullptr) {
    if (shape && transform) {
        // vector and CPU clips must use the same owner coordinate system.
        shape->transformed = rdt_matrix_inverse(transform, &shape->inverse_transform);
        if (!shape->transformed) shape = nullptr;
    }
    if (!shape || !render_clip_push_shape_scope(rdcon, scope, shape, transform)) {
        if (rdcon) scratch_scope_end(&rdcon->scratch, mem);
        return false;
    }
    scope->owns_shape = true;
    scope->mem = *mem;
    return true;
}

RenderClipScope render_clip_push_css_scope(RasterRenderContext* rdcon, ViewBlock* block,
                                           float parent_x, float parent_y, float scale) {
    RenderClipScope scope = {};
    if (!rdcon || !block) {
        return scope;
    }
    const char* clip_str = render_clip_css_value(block);
    if (!clip_str || strncmp(clip_str, "none", 4) == 0) {
        return scope;
    }

    float elem_w = block->width * scale;
    float elem_h = block->height * scale;
    float abs_x = parent_x + block->x * scale;
    float abs_y = parent_y + block->y * scale;
    RdtFillRule rule = RDT_FILL_WINDING;
    RdtPath* css_path = render_clip_parse_path_function(clip_str, elem_w, elem_h, abs_x, abs_y, &rule);
    if (css_path) {
        rc_push_clip(rdcon, css_path, render_state_current_transform(rdcon), rule);
        rdt_path_free(css_path);
        scope.active = true;
        log_debug("[CLIP] CSS clip-path path(): %s on element %s", clip_str, block->node_name());
        return scope;
    }

    ScratchMark mem = scratch_scope_begin(&rdcon->scratch);
    ClipShape* css_shape = render_clip_parse_css_shape(&rdcon->scratch, &mem, block,
                                                       elem_w, elem_h, abs_x, abs_y, scale);
    if (!render_clip_push_owned_shape(rdcon, &scope, css_shape, &mem,
            render_state_current_transform(rdcon))) return scope;
    log_debug("[CLIP] CSS clip-path: %s on element %s", clip_str, block->node_name());
    return scope;
}

bool render_clip_push_vector_css(PaintList* paint, ViewElement* element,
                                 float abs_x, float abs_y) {
    const char* value = render_clip_css_value(element);
    if (!paint || !value || strncmp(value, "none", 4) == 0) return false;

    RdtFillRule rule = RDT_FILL_WINDING;
    RdtPath* path = render_clip_parse_path_function(value, element->width, element->height, abs_x, abs_y, &rule);
    if (!path) {
        Arena* backing = arena_create_default();
        if (!backing) return false;
        ScratchArena scratch = {};
        scratch_init(&scratch, backing);
        ScratchMark scope = scratch_scope_begin(&scratch);
        ClipShape* shape = render_clip_parse_css_shape(&scratch, &scope, element,
            element->width, element->height, abs_x, abs_y, 1.0f);
        if (shape) path = render_clip_create_shape_path(shape);
        scratch_scope_end(&scratch, &scope);
        scratch_release(&scratch);
        arena_destroy(backing);
    }
    if (!path) return false;
    int before = paint_list_count(paint);
    paint_push_clip(paint, path, nullptr, rule);
    rdt_path_free(path);
    return paint_list_count(paint) > before;
}

bool render_clip_legacy_geometry(ViewBlock* block, Bound* bounds) {
    if (!block || !bounds || !block->position ||
        !layout_block_is_out_of_flow_positioned(block) || !block->positionp()->clip.active) return false;
    const LegacyClipRect& clip = block->positionp()->clip;
    float edges[4];
    const float defaults[] = {0.0f, block->width, block->height, 0.0f};
    for (size_t side = 0; side < 4; side++)
        edges[side] = clip.auto_edges & (1u << side) ? defaults[side] : clip.edges[side];
    *bounds = {edges[3], edges[0], fmaxf(edges[3], edges[1]), fmaxf(edges[0], edges[2])};
    return true;
}

RenderClipScope render_clip_push_legacy_scope(RasterRenderContext* context, ViewBlock* block,
        float parent_x, float parent_y, float scale) {
    Bound clip;
    if (!context || !render_clip_legacy_geometry(block, &clip)) return {};
    float x = parent_x + block->x * scale, y = parent_y + block->y * scale;
    ScratchMark memory = scratch_scope_begin(&context->scratch);
    ClipShape* shape = render_clip_alloc_shape(&context->scratch, &memory);
    if (shape) {
        shape->type = CLIP_SHAPE_INSET;
        shape->inset = {x + clip.left * scale, y + clip.top * scale,
            (clip.right - clip.left) * scale, (clip.bottom - clip.top) * scale, 0.0f, 0.0f};
    }
    RenderClipScope scope = {};
    render_clip_push_owned_shape(context, &scope, shape, &memory, render_state_current_transform(context));
    return scope;
}

bool render_clip_overflow_geometry(ViewBlock* block, Bound* clip, Corner* radius) {
    if (!clip || !radius || !layout_block_overflow_clip(block, clip)) return false;
    *radius = {};
    // CSS Overflow 3 §3.1.2: a clip/visible pair has straight clipping edges.
    if (!layout_block_clips_overflow_axis(block, true) ||
        !layout_block_clips_overflow_axis(block, false)) return true;
    BorderProp* border = block->bound ? block->boundary()->border.get() : nullptr;
    if (border) {
        resolve_border_radius_percentages(&border->radius, block->width, block->height);
        *radius = render_geometry_outset_corners(&border->radius, block->width, block->height,
            -clip->top, clip->right - block->width, clip->bottom - block->height, -clip->left);
        constrain_corner_radii(radius, clip->right - clip->left, clip->bottom - clip->top);
    }
    return true;
}

bool render_clip_overflow_bounds(ViewBlock* block, float origin_x, float origin_y,
        float scale, Bound visible, const RdtMatrix* transform, Bound* clip, Corner* radius) {
    if (!render_clip_overflow_geometry(block, clip, radius)) return false;
    clip->left = origin_x + clip->left * scale;
    clip->right = origin_x + clip->right * scale;
    clip->top = origin_y + clip->top * scale;
    clip->bottom = origin_y + clip->bottom * scale;
    *radius = radiant_corner_scaled(radius, scale);
    bool clip_x = layout_block_clips_overflow_axis(block, true);
    bool clip_y = layout_block_clips_overflow_axis(block, false);
    if (clip_x && clip_y) return true;
    // bound an open strip by the destination clip in owner space, without imposing a box edge.
    if (transform) {
        RdtMatrix inverse;
        if (!rdt_matrix_inverse(transform, &inverse) ||
            !rdt_matrix_project_rect_bounds(&inverse, visible.left, visible.top,
                visible.right, visible.bottom, &visible.left, &visible.top,
                &visible.right, &visible.bottom)) {
            log_error("[OVERFLOW_CLIP] cannot bound an open axis across a projective horizon");
            return false;
        }
    }
    if (!clip_x) { clip->left = visible.left; clip->right = visible.right; }
    if (!clip_y) { clip->top = visible.top; clip->bottom = visible.bottom; }
    return true;
}

bool render_clip_push_vector(PaintList* paint, ViewElement* element,
        float abs_x, float abs_y, RenderClipKind kind, Bound visible,
        const RdtMatrix* transform) {
    if (kind == RENDER_CLIP_SHAPE) return render_clip_push_vector_css(paint, element, abs_x, abs_y);
    ViewBlock* block = lam::view_as_block(element);
    Bound clip; Corner radius;
    if (!paint) return false;
    if (kind == RENDER_CLIP_LEGACY) {
        if (!render_clip_legacy_geometry(block, &clip)) return false;
        clip = {abs_x + clip.left, abs_y + clip.top, abs_x + clip.right, abs_y + clip.bottom};
        radius = {};
    } else if (!render_clip_overflow_bounds(block, abs_x, abs_y, 1.0f,
            visible, transform, &clip, &radius)) return false;
    RdtPath* path = render_path_create_rounded_rect(
        {clip.left, clip.top, clip.right - clip.left, clip.bottom - clip.top}, &radius);
    if (!path) return false;
    int before = paint_list_count(paint);
    paint_push_clip(paint, path, nullptr, RDT_FILL_WINDING);
    rdt_path_free(path);
    return paint_list_count(paint) > before;
}

RenderClipScope render_clip_push_rect_scope(RasterRenderContext* rdcon, const Bound* clip,
                                           const RdtMatrix* transform) {
    RenderClipScope scope = {};
    if (!rdcon || !clip) {
        return scope;
    }
    float w = clip->right - clip->left;
    float h = clip->bottom - clip->top;
    if (w <= 0 || h <= 0) {
        return scope;
    }

    ClipShape inline_shape = {};
    inline_shape.type = CLIP_SHAPE_INSET;
    inline_shape.inset = {clip->left, clip->top, w, h, 0, 0};
    ScratchMark mem = scratch_scope_begin(&rdcon->scratch);
    ClipShape* shape = render_clip_clone_shape(&rdcon->scratch, &mem, &inline_shape);
    if (!render_clip_push_owned_shape(rdcon, &scope, shape, &mem, transform)) return scope;
    log_debug("[CLIP] pushed rect clip: (%.0f,%.0f) %.0fx%.0f",
        clip->left, clip->top, w, h);
    return scope;
}

RenderClipScope render_clip_push_overflow_scope(RasterRenderContext* rdcon, ViewBlock* block,
                                               float origin_x, float origin_y) {
    RenderClipScope scope = {};
    Bound clip; Corner radius;
    if (!rdcon || !render_clip_overflow_bounds(block, origin_x, origin_y,
            rdcon->raster_scale, rdcon->block.clip, render_state_current_transform(rdcon),
            &clip, &radius)) return scope;
    // untransformed rectangles already constrain every primitive through the block bound;
    // a second vector mask changes image edge coverage without adding a constraint.
    if (!radiant_corner_has_radius(&radius) && !render_state_current_transform(rdcon)) return scope;
    float cw = clip.right - clip.left;
    float ch = clip.bottom - clip.top;
    if (cw <= 0.0f || ch <= 0.0f) return scope;
    Corner cr = radius;

    ClipShape inline_shape = {};
    inline_shape.type = CLIP_SHAPE_ROUNDED_RECT;
    // retain the owner's shape before scrolling or intersection with ancestor clips.
    inline_shape.rounded_rect = {clip.left, clip.top, cw, ch,
        cr.top_left, cr.top_right, cr.bottom_right, cr.bottom_left};
    ScratchMark mem = scratch_scope_begin(&rdcon->scratch);
    ClipShape* shape = render_clip_clone_shape(&rdcon->scratch, &mem, &inline_shape);
    if (!render_clip_push_owned_shape(rdcon, &scope, shape, &mem,
            render_state_current_transform(rdcon))) return scope;

    // Clear the flag so child elements do not redundantly push the same clip.
    rdcon->block.has_clip_radius = false;
    log_debug("[CLIP] pushed overflow vector clip: (%.0f,%.0f) %.0fx%.0f r=[%.0f,%.0f,%.0f,%.0f]",
        inline_shape.rounded_rect.x, inline_shape.rounded_rect.y, cw, ch,
        cr.top_left, cr.top_right, cr.bottom_right, cr.bottom_left);
    return scope;
}

RenderClipScope render_clip_push_viewport_scope(RasterRenderContext* rdcon, ViewBlock* block,
                                               RdtLogicalPoint document_offset) {
    RenderClipScope scope = {};
    RdtMatrix client;
    if (!rdcon || !block || !view_get_client_matrix(block, &client)) return scope;
    RdtMatrix offset = rdt_matrix_translate(document_offset.x, document_offset.y);
    RdtMatrix density = rdt_matrix_scale(rdcon->raster_scale, rdcon->raster_scale);
    RdtMatrix window = rdt_matrix_multiply(&offset, &client);
    RdtMatrix device = rdt_matrix_multiply(&density, &window);
    Bound clip; Corner radius;
    if (!render_clip_overflow_bounds(block, 0.0f, 0.0f, 1.0f,
            rdcon->block.clip, &device, &clip, &radius)) return scope;
    // overlays have left normal traversal; replay the owner's unscrolled clip in window space.
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_ROUNDED_RECT;
    shape.rounded_rect = {clip.left, clip.top, max(0.0f, clip.right - clip.left),
        max(0.0f, clip.bottom - clip.top), radius.top_left, radius.top_right,
        radius.bottom_right, radius.bottom_left};
    ScratchMark mem = scratch_scope_begin(&rdcon->scratch);
    render_clip_push_owned_shape(rdcon, &scope,
        render_clip_clone_shape(&rdcon->scratch, &mem, &shape), &mem, &device);
    return scope;
}

void render_clip_pop_scope(RasterRenderContext* rdcon, RenderClipScope* scope) {
    if (!rdcon || !scope || !scope->active) {
        return;
    }
    rc_pop_clip(rdcon);
    if (scope->pushed_shape && rdcon->clip_shape_depth > 0) {
        rdcon->clip_shape_depth--;
    }
    if (scope->owns_shape) {
        scratch_scope_end(&rdcon->scratch, &scope->mem);
    }
    *scope = {};
}
