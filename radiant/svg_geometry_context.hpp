#pragma once

#include "render.hpp"
#include "../lib/hashmap.h"

// Synchronous SVG hit walks borrow a linked stack of instance style scopes.
extern thread_local const SvgDomStyleScope* g_dom_svg_style_scope;

struct DomSvgHitGeometry {
    DomElement* element;
    SvgLengthContext lengths;
    RdtMatrix local;
    RdtMatrix screen;
    RdtMatrix viewport;
    bool has_lengths, has_local, has_screen, has_viewport;
};

struct DomSvgHitGeometryScope {
    DomDocument* document;
    HashMap* entries;
    DomSvgHitGeometryScope* previous;
    bool owner;
    explicit DomSvgHitGeometryScope(DomElement* element);
    ~DomSvgHitGeometryScope();
};

DomSvgHitGeometry* dom_svg_hit_geometry(DomElement* element, bool create = false);
void dom_svg_apply_css_viewport_size(DomElement* element, float* width, float* height);

extern "C" bool dom_svg_foreign_object_rectangle(void* element, float* x, float* y,
                                                  float* width, float* height);
extern "C" bool dom_svg_foreign_object_clips(void* element);

// Geometry is shared by native rendering and DOM adapters.
struct SvgGeometryBounds {
    float left;
    float top;
    float right;
    float bottom;
    bool valid;
};
void dom_svg_bounds_apply_transform(SvgGeometryBounds* bounds, const RdtMatrix* transform);
float dom_svg_attribute_number(DomElement* element, const char* name, float fallback);
RdtMatrix dom_svg_transform_from_element(DomElement* element);
extern "C" bool dom_svg_is_text(void* element);
RdtPath* dom_svg_text_geometry_path(DomElement* element);
bool dom_svg_is_basic_shape(DomElement* element);
RdtPath* dom_svg_basic_shape_path(DomElement* element);
DomElement* dom_svg_use_reference(DomElement* element);
SvgGeometryBounds dom_svg_bounds_for_element(DomElement* element, int depth = 0);
extern "C" bool dom_svg_element_geometry_bounds(void* element, float* left, float* top,
                                                float* right, float* bottom);

bool dom_svg_parse_viewbox(const char* value, float* min_x, float* min_y,
                           float* width, float* height);
RdtMatrix dom_svg_ctm(DomElement* element, bool screen_space);
extern "C" bool dom_svg_foreign_object_client_transform(void* element, float matrix[6]);
extern "C" bool dom_svg_foreign_object_local_point(void* element, float x, float y,
                                                  float* local_x, float* local_y);
extern "C" bool dom_svg_element_client_bounds(void* element, float* x, float* y,
                                              float* width, float* height);
