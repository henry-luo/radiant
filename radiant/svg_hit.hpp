#pragma once

#include "render.hpp"

typedef struct SvgPathHitContext {
    float point_x;
    float point_y;
    float curve_flatness;
    bool rectangle_query, rectangle_fill_hit, subpath_rectangle_fill_hit;
    float query_left, query_top, query_right, query_bottom;
    float stroke_radius;
    float* stroke_dash;          // query-owned computed lengths; released after contour inspection
    float stroke_dash_total;
    float stroke_dash_offset;
    float subpath_stroke_length;
    float first_stroke_start_x;
    float first_stroke_start_y;
    float first_stroke_end_x;
    float first_stroke_end_y;
    float last_stroke_start_x;
    float last_stroke_start_y;
    float last_stroke_end_x;
    float last_stroke_end_y;
    int stroke_dash_count;
    RdtStrokeCap stroke_cap;
    RdtStrokeJoin stroke_join;
    float stroke_miter_limit;
    int fill_winding;
    int fill_crossings;
    bool fill_on_edge;
    bool subpath_fill_on_edge;
    bool has_current;
    bool subpath_has_draw;
    bool subpath_closed;
    bool has_first_stroke_segment;
    bool has_last_stroke_segment;
    bool stroke_enabled;
    bool stroke_hit;
    bool subpath_has_fill_area;
    float current_x;
    float current_y;
    float subpath_start_x;
    float subpath_start_y;
} SvgPathHitContext;

void dom_svg_path_hit_finish_subpath(SvgPathHitContext* context);
bool dom_svg_path_hit_visit(void* userdata, RdtPathCommand command, const float* args, int arg_count);

extern "C" bool dom_geometry_path_query(const void* path, float left, float top, float right, float bottom,
    bool rectangle, int fill_rule, float stroke_width, int cap, int join);

// Native geometric hit testing is shared by Radiant and Lambda DOM.
extern "C" void* dom_svg_element_from_point(void* element, float x, float y);
extern "C" void dom_svg_dispatch_use_timing_event(void* element, float x, float y, const char* type, bool bubbles, double detail);
