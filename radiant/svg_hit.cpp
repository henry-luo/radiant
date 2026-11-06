#include "svg_hit.hpp"
#include <float.h>
#include <math.h>

static const int SVG_PATH_HIT_MAX_CUBIC_DEPTH = 10;




// separating axes cover both a point and an axis-aligned rectangle query.
static bool dom_svg_query_polygon(const SvgPathHitContext* query, const float* xy, unsigned count) {
    float left=query->rectangle_query?query->query_left:query->point_x;
    float top=query->rectangle_query?query->query_top:query->point_y;
    float right=query->rectangle_query?query->query_right:left;
    float bottom=query->rectangle_query?query->query_bottom:top;
    for (unsigned axis=0;axis<count+2;axis++) {
        unsigned next=(axis+1)%count;
        float nx=axis==count?1:axis==count+1?0:xy[next*2+1]-xy[axis*2+1];
        float ny=axis==count?0:axis==count+1?1:xy[axis*2]-xy[next*2];
        float low=INFINITY,high=-INFINITY;
        for (unsigned i=0;i<count;i++) { float value=nx*xy[i*2]+ny*xy[i*2+1]; low=fminf(low,value);high=fmaxf(high,value); }
        float qlow=fminf(nx*left,nx*right)+fminf(ny*top,ny*bottom);
        float qhigh=fmaxf(nx*left,nx*right)+fmaxf(ny*top,ny*bottom);
        if (high<qlow || low>qhigh) return false;
    }
    return true;
}
static bool dom_svg_query_triangle(const SvgPathHitContext* query,
    float ax,float ay,float bx,float by,float cx,float cy) {
    float xy[]={ax,ay,bx,by,cx,cy}; return dom_svg_query_polygon(query,xy,3);
}
static bool dom_svg_query_circle(const SvgPathHitContext* query,float x,float y,float radius) {
    float px=query->rectangle_query?fmaxf(query->query_left,fminf(query->query_right,x)):query->point_x;
    float py=query->rectangle_query?fmaxf(query->query_top,fminf(query->query_bottom,y)):query->point_y;
    return (px-x)*(px-x)+(py-y)*(py-y)<=radius*radius;
}

static bool dom_svg_stroke_segment_contains(const SvgPathHitContext* context,
                                                float start_x, float start_y,
                                                float end_x, float end_y,
                                                bool start_cap, bool end_cap) {
    if (!context) return false;
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float length = hypotf(dx, dy);
    if (length <= 0.000001f) {
        if (context->stroke_cap == RDT_CAP_BUTT || (!start_cap && !end_cap)) return false;
        if (context->rectangle_query) {
            if (context->stroke_cap==RDT_CAP_ROUND) return dom_svg_query_circle(context,start_x,start_y,context->stroke_radius);
            float r=context->stroke_radius,quad[]={start_x-r,start_y-r,start_x+r,start_y-r,start_x+r,start_y+r,start_x-r,start_y+r};
            return dom_svg_query_polygon(context,quad,4);
        }
        float px = context->point_x - start_x;
        float py = context->point_y - start_y;
        if (context->stroke_cap == RDT_CAP_SQUARE)
            return fabsf(px) <= context->stroke_radius && fabsf(py) <= context->stroke_radius;
        return px * px + py * py <= context->stroke_radius * context->stroke_radius;
    }
    float ux = dx / length;
    float uy = dy / length;
    if (context->rectangle_query) {
        float before=context->stroke_cap==RDT_CAP_SQUARE && start_cap?context->stroke_radius:0;
        float after=context->stroke_cap==RDT_CAP_SQUARE && end_cap?context->stroke_radius:0;
        float sx=start_x-ux*before,sy=start_y-uy*before,ex=end_x+ux*after,ey=end_y+uy*after;
        float nx=-uy*context->stroke_radius,ny=ux*context->stroke_radius;
        float quad[]={sx+nx,sy+ny,ex+nx,ey+ny,ex-nx,ey-ny,sx-nx,sy-ny};
        if (dom_svg_query_polygon(context,quad,4)) return true;
        return context->stroke_cap==RDT_CAP_ROUND &&
            ((start_cap && dom_svg_query_circle(context,start_x,start_y,context->stroke_radius)) ||
             (end_cap && dom_svg_query_circle(context,end_x,end_y,context->stroke_radius)));
    }
    float point_dx = context->point_x - start_x;
    float point_dy = context->point_y - start_y;
    float along = point_dx * ux + point_dy * uy;
    float across = point_dx * -uy + point_dy * ux;
    if (fabsf(across) > context->stroke_radius) return false;
    float min_along = 0.0f;
    float max_along = length;
    if (context->stroke_cap == RDT_CAP_SQUARE) {
        if (start_cap) min_along -= context->stroke_radius;
        if (end_cap) max_along += context->stroke_radius;
    }
    if (along >= min_along && along <= max_along) return true;
    if (context->stroke_cap != RDT_CAP_ROUND) return false;
    if (start_cap && along < 0.0f) {
        return point_dx * point_dx + point_dy * point_dy <=
            context->stroke_radius * context->stroke_radius;
    }
    if (end_cap && along > length) {
        float end_dx = context->point_x - end_x;
        float end_dy = context->point_y - end_y;
        return end_dx * end_dx + end_dy * end_dy <=
            context->stroke_radius * context->stroke_radius;
    }
    return false;
}

static bool dom_svg_dash_is_on_at(const SvgPathHitContext* context,
                                     float distance, float* out_remaining) {
    if (!context || context->stroke_dash_count <= 0 || context->stroke_dash_total <= 0.0f) {
        if (out_remaining) *out_remaining = 1.0e30f;
        return true;
    }
    float position = math_wrap_positive_f(distance + context->stroke_dash_offset,
        context->stroke_dash_total);
    for (int index = 0; index < context->stroke_dash_count; index++) {
        float length = context->stroke_dash[index];
        if (position < length || index == context->stroke_dash_count - 1) {
            if (out_remaining) *out_remaining = LMB_MAX(length - position, 0.0f);
            return (index & 1) == 0;
        }
        position -= length;
    }
    if (out_remaining) *out_remaining = 0.0f;
    return false;
}

static void dom_svg_path_hit_add_join(SvgPathHitContext* context,
                                         float prev_start_x, float prev_start_y,
                                         float vertex_x, float vertex_y,
                                         float next_end_x, float next_end_y) {
    if (!context || context->stroke_hit) return;
    float prev_dx = vertex_x - prev_start_x;
    float prev_dy = vertex_y - prev_start_y;
    float next_dx = next_end_x - vertex_x;
    float next_dy = next_end_y - vertex_y;
    float prev_length = hypotf(prev_dx, prev_dy);
    float next_length = hypotf(next_dx, next_dy);
    if (prev_length <= 0.000001f || next_length <= 0.000001f) return;
    if (context->stroke_join == RDT_JOIN_ROUND &&
        dom_svg_query_circle(context,vertex_x,vertex_y,context->stroke_radius)) {
        context->stroke_hit = true;
        return;
    }
    float prev_x = prev_dx / prev_length;
    float prev_y = prev_dy / prev_length;
    float next_x = next_dx / next_length;
    float next_y = next_dy / next_length;
    float cross = prev_x * next_y - prev_y * next_x;
    if (fabsf(cross) <= 0.000001f) return;
    float outward_sign = cross > 0.0f ? -1.0f : 1.0f;
    float prev_normal_x = -prev_y * outward_sign;
    float prev_normal_y = prev_x * outward_sign;
    float next_normal_x = -next_y * outward_sign;
    float next_normal_y = next_x * outward_sign;
    float outer_prev_x = vertex_x + prev_normal_x * context->stroke_radius;
    float outer_prev_y = vertex_y + prev_normal_y * context->stroke_radius;
    float outer_next_x = vertex_x + next_normal_x * context->stroke_radius;
    float outer_next_y = vertex_y + next_normal_y * context->stroke_radius;
    // segment strips already cover the inner corner; the outer bevel triangle is common to bevel and miter joins.
    context->stroke_hit = dom_svg_query_triangle(context,
        vertex_x, vertex_y, outer_prev_x, outer_prev_y, outer_next_x, outer_next_y);
    if (context->stroke_hit || context->stroke_join != RDT_JOIN_MITER) return;
    float determinant = prev_x * -next_y - prev_y * -next_x;
    if (fabsf(determinant) <= 0.000001f) return;
    float delta_x = outer_next_x - outer_prev_x;
    float delta_y = outer_next_y - outer_prev_y;
    float prev_distance = (delta_x * -next_y - delta_y * -next_x) / determinant;
    float miter_x = outer_prev_x + prev_x * prev_distance;
    float miter_y = outer_prev_y + prev_y * prev_distance;
    float miter_length = hypotf(miter_x - vertex_x, miter_y - vertex_y);
    if (miter_length > context->stroke_miter_limit * context->stroke_radius) {
        context->stroke_hit = dom_svg_query_triangle(context,
            vertex_x, vertex_y, outer_prev_x, outer_prev_y, outer_next_x, outer_next_y);
        return;
    }
    context->stroke_hit = dom_svg_query_triangle(context,
        outer_prev_x, outer_prev_y, outer_next_x, outer_next_y, miter_x, miter_y);
}

static void dom_svg_path_hit_add_stroke_segment(SvgPathHitContext* context,
                                                    float start_x, float start_y,
                                                    float end_x, float end_y) {
    if (!context || !context->stroke_enabled) return;
    if (context->stroke_dash_count == 0 && context->has_last_stroke_segment) {
        dom_svg_path_hit_add_join(context, context->last_stroke_start_x,
            context->last_stroke_start_y, start_x, start_y, end_x, end_y);
    }
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float length = hypotf(dx, dy);
    if (length <= 0.000001f) {
        if (context->stroke_dash_count == 0 && dom_svg_stroke_segment_contains(context,
            start_x, start_y, end_x, end_y, true, true)) context->stroke_hit = true;
        return;
    }
    if (!context->has_first_stroke_segment) {
        context->first_stroke_start_x = start_x;
        context->first_stroke_start_y = start_y;
        context->first_stroke_end_x = end_x;
        context->first_stroke_end_y = end_y;
        context->has_first_stroke_segment = true;
    }
    if (context->stroke_dash_count <= 0) {
        if (dom_svg_stroke_segment_contains(context, start_x, start_y,
                end_x, end_y, false, false)) {
            context->stroke_hit = true;
        }
    } else {
        // zero-length on-dashes have cap geometry even though their interval is empty.
        if (context->stroke_cap != RDT_CAP_BUTT && context->stroke_dash_total > 0.0f) {
            float cumulative = 0.0f;
            for (int index = 0; index < context->stroke_dash_count; index++) {
                if (!(index & 1) && context->stroke_dash[index] == 0.0f) {
                    float base = cumulative - context->stroke_dash_offset;
                    float start = context->subpath_stroke_length;
                    float first = base + ceilf((start - base) / context->stroke_dash_total) * context->stroke_dash_total;
                    for (float distance = first; distance <= start + length; distance += context->stroke_dash_total) {
                        float fraction = (distance - start) / length;
                        float x = start_x + dx * fraction;
                        float y = start_y + dy * fraction;
                        if (dom_svg_stroke_segment_contains(context, x, y, x, y, true, true)) context->stroke_hit = true;
                    }
                }
                cumulative += context->stroke_dash[index];
            }
        }
        float segment_offset = 0.0f;
        while (segment_offset < length) {
            float remaining = 0.0f;
            float path_offset = context->subpath_stroke_length + segment_offset;
            bool dash_on = dom_svg_dash_is_on_at(context, path_offset, &remaining);
            // Inexact dash lengths (3.2, 2.4) put a boundary a few float steps
            // from the offset; a step that small cannot advance an offset of
            // this magnitude, and the walk spun forever. At a boundary the next
            // dash's state applies, measured from just past it.
            float min_step = LMB_MAX(0.000001f,
                (fabsf(path_offset) + length) * 4.0f * FLT_EPSILON);
            if (remaining <= min_step) {
                dash_on = dom_svg_dash_is_on_at(context, path_offset + min_step, &remaining);
                remaining += min_step;
            }
            if (remaining <= min_step) remaining = length - segment_offset;
            float fragment_length = LMB_MIN(remaining, length - segment_offset);
            if (dash_on) {
                float start_fraction = segment_offset / length;
                float end_fraction = (segment_offset + fragment_length) / length;
                float fragment_start_x = start_x + dx * start_fraction;
                float fragment_start_y = start_y + dy * start_fraction;
                float fragment_end_x = start_x + dx * end_fraction;
                float fragment_end_y = start_y + dy * end_fraction;
                bool start_cap = path_offset <= 0.000001f ||
                    !dom_svg_dash_is_on_at(context, path_offset - 0.0001f, nullptr);
                bool end_cap = !dom_svg_dash_is_on_at(context,
                    path_offset + fragment_length + 0.0001f, nullptr);
                if (dom_svg_stroke_segment_contains(context, fragment_start_x, fragment_start_y,
                        fragment_end_x, fragment_end_y, start_cap, end_cap)) {
                    context->stroke_hit = true;
                }
            }
            segment_offset += fragment_length;
        }
    }
    context->subpath_stroke_length += length;
    context->last_stroke_start_x = start_x;
    context->last_stroke_start_y = start_y;
    context->last_stroke_end_x = end_x;
    context->last_stroke_end_y = end_y;
    context->has_last_stroke_segment = true;
}

static void dom_svg_path_hit_add_fill_edge(SvgPathHitContext* context,
                                              float start_x, float start_y,
                                              float end_x, float end_y) {
    if (!context) return;
    if (context->rectangle_query) {
        float edge[]={start_x,start_y,end_x,end_y};
        context->subpath_rectangle_fill_hit|=dom_svg_query_polygon(context,edge,2);
    }
    if (fabsf(end_y - start_y) <= 0.000001f) return;
    bool upward = start_y <= context->point_y && end_y > context->point_y;
    bool downward = start_y > context->point_y && end_y <= context->point_y;
    if (!upward && !downward) return;
    float side = (end_x - start_x) * (context->point_y - start_y) -
        (context->point_x - start_x) * (end_y - start_y);
    if (fabsf(side) <= 0.000001f) {
        context->subpath_fill_on_edge = true;
        return;
    }
    if (upward && side > 0.0f) {
        context->fill_winding++;
        context->fill_crossings++;
    } else if (downward && side < 0.0f) {
        context->fill_winding--;
        context->fill_crossings++;
    }
}

static void dom_svg_path_hit_add_segment(SvgPathHitContext* context,
                                            float start_x, float start_y,
                                            float end_x, float end_y,
                                            bool include_stroke) {
    if (!context) return;
    // self-crossing contours can cancel their signed area while still enclosing filled lobes.
    float ax=start_x-context->subpath_start_x,ay=start_y-context->subpath_start_y;
    float bx=end_x-context->subpath_start_x,by=end_y-context->subpath_start_y;
    context->subpath_has_fill_area |= fabsf(ax*by-ay*bx)>0.000001f;
    if (include_stroke) {
        dom_svg_path_hit_add_stroke_segment(context, start_x, start_y, end_x, end_y);
    }
    dom_svg_path_hit_add_fill_edge(context, start_x, start_y, end_x, end_y);
}

static void dom_svg_path_hit_commit_subpath_fill_edge(SvgPathHitContext* context) {
    if (!context) return;
    if (context->subpath_has_fill_area) {
        context->fill_on_edge |= context->subpath_fill_on_edge;
        context->rectangle_fill_hit |= context->subpath_rectangle_fill_hit;
    }
}

void dom_svg_path_hit_finish_subpath(SvgPathHitContext* context) {
    if (!context || !context->has_current || !context->subpath_has_draw) return;
    if (!context->subpath_closed) {
        // SVG fill implicitly closes an open subpath, while its stroke does not.
        dom_svg_path_hit_add_segment(context, context->current_x, context->current_y,
                                        context->subpath_start_x, context->subpath_start_y,
                                        false);
        // A zero-area contour, such as <line>, has no fill boundary to pick.
        dom_svg_path_hit_commit_subpath_fill_edge(context);
    }
    if (!context->subpath_closed && context->has_first_stroke_segment &&
        context->stroke_dash_count == 0) {
        if (dom_svg_stroke_segment_contains(context, context->first_stroke_start_x,
                context->first_stroke_start_y, context->first_stroke_end_x,
                context->first_stroke_end_y, true, false) ||
            dom_svg_stroke_segment_contains(context, context->last_stroke_start_x,
                context->last_stroke_start_y, context->last_stroke_end_x,
                context->last_stroke_end_y, false, true)) {
            context->stroke_hit = true;
        }
    }
}

static bool dom_svg_path_hit_curve_line(void* data, float x0, float y0, float x1, float y1) {
    dom_svg_path_hit_add_segment((SvgPathHitContext*)data, x0, y0, x1, y1, true);
    return true;
}

static void dom_svg_path_hit_flatten_cubic(SvgPathHitContext* context,
    float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3) {
    render_path_flatten_cubic(x0, y0, x1, y1, x2, y2, x3, y3, context->curve_flatness,
        SVG_PATH_HIT_MAX_CUBIC_DEPTH, dom_svg_path_hit_curve_line, context);
}

bool dom_svg_path_hit_visit(void* userdata, RdtPathCommand command,
                                      const float* args, int arg_count) {
    SvgPathHitContext* context = (SvgPathHitContext*)userdata;
    if (!context) return false;
    switch (command) {
    case RDT_PATH_MOVE:
        if (arg_count < 2) return false;
        dom_svg_path_hit_finish_subpath(context);
        context->current_x = args[0];
        context->current_y = args[1];
        context->subpath_start_x = args[0];
        context->subpath_start_y = args[1];
        context->has_current = true;
        context->subpath_has_draw = false;
        context->subpath_closed = false;
        context->subpath_stroke_length = 0.0f;
        context->has_first_stroke_segment = false;
        context->has_last_stroke_segment = false;
        context->subpath_has_fill_area = false;
        context->subpath_fill_on_edge = false;
        context->subpath_rectangle_fill_hit = false;
        return true;
    case RDT_PATH_LINE:
        if (arg_count < 2 || !context->has_current) return false;
        dom_svg_path_hit_add_segment(context, context->current_x, context->current_y,
                                        args[0], args[1], true);
        context->current_x = args[0];
        context->current_y = args[1];
        context->subpath_has_draw = true;
        context->subpath_closed = false;
        return true;
    case RDT_PATH_QUAD:
        if (arg_count < 4 || !context->has_current) return false;
        dom_svg_path_hit_flatten_cubic(context, context->current_x, context->current_y,
            context->current_x + (args[0] - context->current_x) * (2.0f / 3.0f),
            context->current_y + (args[1] - context->current_y) * (2.0f / 3.0f),
            args[2] + (args[0] - args[2]) * (2.0f / 3.0f),
            args[3] + (args[1] - args[3]) * (2.0f / 3.0f),
            args[2], args[3]);
        context->current_x = args[2];
        context->current_y = args[3];
        context->subpath_has_draw = true;
        context->subpath_closed = false;
        return true;
    case RDT_PATH_CUBIC:
        if (arg_count < 6 || !context->has_current) return false;
        dom_svg_path_hit_flatten_cubic(context, context->current_x, context->current_y,
            args[0], args[1], args[2], args[3], args[4], args[5]);
        context->current_x = args[4];
        context->current_y = args[5];
        context->subpath_has_draw = true;
        context->subpath_closed = false;
        return true;
    case RDT_PATH_CLOSE:
        if (!context->has_current || !context->subpath_has_draw) return true;
        dom_svg_path_hit_add_segment(context, context->current_x, context->current_y,
                                        context->subpath_start_x, context->subpath_start_y,
                                        true);
        if (context->stroke_dash_count == 0 && context->has_first_stroke_segment &&
            context->has_last_stroke_segment) {
            dom_svg_path_hit_add_join(context, context->last_stroke_start_x,
                context->last_stroke_start_y, context->subpath_start_x,
                context->subpath_start_y, context->first_stroke_end_x,
                context->first_stroke_end_y);
        }
        context->current_x = context->subpath_start_x;
        context->current_y = context->subpath_start_y;
        context->subpath_closed = true;
        dom_svg_path_hit_commit_subpath_fill_edge(context);
        return true;
    case RDT_PATH_RECT:
    case RDT_PATH_CIRCLE:
        // SVG path data is lowered to move/line/cubic commands before this
        // visitor. Other backend-only path primitives have no SVG `d` source.
        return false;
    }
    return false;
}

extern "C" bool dom_geometry_path_query(const void* opaque_path, float left,float top,float right,float bottom,
    bool rectangle, int fill_rule,float stroke_width,int cap,int join) {
    const RdtPath* path=(const RdtPath*)opaque_path;
    RdtPath* contour=rdt_path_new(); RdtMatrix identity=rdt_matrix_identity();
    if (!contour || !render_path_append_transformed(contour,path,&identity)) {
        if (contour) rdt_path_free(contour); return false;
    }
    SvgPathHitContext query={};
    query.point_x=(left+right)/2;query.point_y=(top+bottom)/2;query.curve_flatness=.01f;
    query.rectangle_query=rectangle;query.query_left=left;query.query_top=top;query.query_right=right;query.query_bottom=bottom;
    query.stroke_enabled=stroke_width>0;query.stroke_radius=stroke_width/2;
    query.stroke_cap=(RdtStrokeCap)cap;query.stroke_join=(RdtStrokeJoin)join;query.stroke_miter_limit=4;
    bool valid=rdt_path_visit(contour,dom_svg_path_hit_visit,&query);
    if (valid) dom_svg_path_hit_finish_subpath(&query);
    bool fill=fill_rule>=0 && (query.rectangle_fill_hit || query.fill_on_edge ||
        (fill_rule==RDT_FILL_EVEN_ODD?(query.fill_crossings&1)!=0:query.fill_winding!=0));
    rdt_path_free(contour);return valid && (fill || query.stroke_hit);
}

