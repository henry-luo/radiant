#include "render_path_internal.hpp"

#define RENDER_PATH_KAPPA 0.5522847498f

void render_path_append_rounded_rect_contour(RdtPath* path, Rect rect,
                                                   const Corner* radius, bool clockwise) {
    // expansion must emit vertices even when a zero-radius box has a compact rectangle form.

    float x = rect.x;
    float y = rect.y;
    float w = rect.width;
    float h = rect.height;

    float rx_tl = radius->top_left;
    float rx_tr = radius->top_right;
    float rx_br = radius->bottom_right;
    float rx_bl = radius->bottom_left;
    float ry_tl = radius->top_left_y;
    float ry_tr = radius->top_right_y;
    float ry_br = radius->bottom_right_y;
    float ry_bl = radius->bottom_left_y;

    if (clockwise) {
        rdt_path_move_to(path, x + rx_tl, y);
        rdt_path_line_to(path, x + w - rx_tr, y);
        if (rx_tr > 0.0f || ry_tr > 0.0f) {
            rdt_path_cubic_to(path,
                x + w - rx_tr + rx_tr * RENDER_PATH_KAPPA, y,
                x + w, y + ry_tr - ry_tr * RENDER_PATH_KAPPA,
                x + w, y + ry_tr);
        }

        rdt_path_line_to(path, x + w, y + h - ry_br);
        if (rx_br > 0.0f || ry_br > 0.0f) {
            rdt_path_cubic_to(path,
                x + w, y + h - ry_br + ry_br * RENDER_PATH_KAPPA,
                x + w - rx_br + rx_br * RENDER_PATH_KAPPA, y + h,
                x + w - rx_br, y + h);
        }

        rdt_path_line_to(path, x + rx_bl, y + h);
        if (rx_bl > 0.0f || ry_bl > 0.0f) {
            rdt_path_cubic_to(path,
                x + rx_bl - rx_bl * RENDER_PATH_KAPPA, y + h,
                x, y + h - ry_bl + ry_bl * RENDER_PATH_KAPPA,
                x, y + h - ry_bl);
        }

        rdt_path_line_to(path, x, y + ry_tl);
        if (rx_tl > 0.0f || ry_tl > 0.0f) {
            rdt_path_cubic_to(path,
                x, y + ry_tl - ry_tl * RENDER_PATH_KAPPA,
                x + rx_tl - rx_tl * RENDER_PATH_KAPPA, y,
                x + rx_tl, y);
        }
    } else {
        rdt_path_move_to(path, x + rx_tl, y);
        if (rx_tl > 0.0f || ry_tl > 0.0f) {
            rdt_path_cubic_to(path,
                x + rx_tl - rx_tl * RENDER_PATH_KAPPA, y,
                x, y + ry_tl - ry_tl * RENDER_PATH_KAPPA,
                x, y + ry_tl);
        }
        rdt_path_line_to(path, x, y + h - ry_bl);
        if (rx_bl > 0.0f || ry_bl > 0.0f) {
            rdt_path_cubic_to(path,
                x, y + h - ry_bl + ry_bl * RENDER_PATH_KAPPA,
                x + rx_bl - rx_bl * RENDER_PATH_KAPPA, y + h,
                x + rx_bl, y + h);
        }
        rdt_path_line_to(path, x + w - rx_br, y + h);
        if (rx_br > 0.0f || ry_br > 0.0f) {
            rdt_path_cubic_to(path,
                x + w - rx_br + rx_br * RENDER_PATH_KAPPA, y + h,
                x + w, y + h - ry_br + ry_br * RENDER_PATH_KAPPA,
                x + w, y + h - ry_br);
        }
        rdt_path_line_to(path, x + w, y + ry_tr);
        if (rx_tr > 0.0f || ry_tr > 0.0f) {
            rdt_path_cubic_to(path,
                x + w, y + ry_tr - ry_tr * RENDER_PATH_KAPPA,
                x + w - rx_tr + rx_tr * RENDER_PATH_KAPPA, y,
                x + w - rx_tr, y);
        }
    }

    rdt_path_close(path);
}

typedef struct {
    RdtPath* destination;
    const RdtMatrix* transform;
    float x, y, start_x, start_y;
} RenderPathTransform;

void render_path_append_quadratic(RdtPath* path, float sx, float sy,
    float qx, float qy, float x, float y) {
    rdt_path_cubic_to(path, sx + 2.0f / 3.0f * (qx - sx), sy + 2.0f / 3.0f * (qy - sy),
        x + 2.0f / 3.0f * (qx - x), y + 2.0f / 3.0f * (qy - y), x, y);
}

static bool render_path_transform_visit(void* context, RdtPathCommand command,
                                         const float* args, int count) {
    RenderPathTransform* writer = (RenderPathTransform*)context;
    if (command == RDT_PATH_CLOSE) {
        rdt_path_close(writer->destination); writer->x = writer->start_x; writer->y = writer->start_y;
        return true;
    }
    if (command == RDT_PATH_RECT || command == RDT_PATH_CIRCLE) {
        Rect rect;
        float rx, ry;
        RdtMatrix mapping = *writer->transform;
        const RdtMatrix* saved_transform = writer->transform;
        if (command == RDT_PATH_RECT) {
            rect = {args[0], args[1], args[2], args[3]}; rx = args[4]; ry = args[5];
        } else {
            // SVG's canonical ellipse starts at the rightmost point, proceeding clockwise.
            rect = {-1.0f, -1.0f, 2.0f, 2.0f}; rx = ry = 1.0f;
            RdtMatrix ellipse = {0.0f, -args[2], args[0], args[3], 0.0f, args[1], 0.0f, 0.0f, 1.0f};
            mapping = rdt_matrix_multiply(saved_transform, &ellipse);
        }
        Corner radius = {};
        for (size_t i = 0; i < 4; i++) { radius.horizontal[i] = rx; radius.vertical[i] = ry; }
        RdtPath* expanded = rdt_path_new();
        if (!expanded) return false;
        // compact primitives need explicit curves before a general affine transform.
        render_path_append_rounded_rect_contour(expanded, rect, &radius, true);
        writer->transform = &mapping;
        bool result = rdt_path_visit(expanded, render_path_transform_visit, context);
        writer->transform = saved_transform;
        rdt_path_free(expanded);
        return result;
    }
    if (command != RDT_PATH_MOVE && command != RDT_PATH_LINE &&
        command != RDT_PATH_QUAD && command != RDT_PATH_CUBIC) return false;
    float values[6];
    for (int i = 0; i < count; i += 2)
        rdt_matrix_transform_point(writer->transform, args[i], args[i + 1], &values[i], &values[i + 1]);
    if (command == RDT_PATH_MOVE) {
        rdt_path_move_to(writer->destination, values[0], values[1]);
        writer->start_x = values[0]; writer->start_y = values[1];
    }
    else if (command == RDT_PATH_LINE) rdt_path_line_to(writer->destination, values[0], values[1]);
    else if (command == RDT_PATH_QUAD) render_path_append_quadratic(writer->destination,
        writer->x, writer->y, values[0], values[1], values[2], values[3]);
    else rdt_path_cubic_to(writer->destination, values[0], values[1], values[2], values[3], values[4], values[5]);
    writer->x = values[count - 2]; writer->y = values[count - 1];
    return true;
}

bool render_path_append_transformed(RdtPath* destination, const RdtPath* source,
                                     const RdtMatrix* transform) {
    if (!destination || !source || !transform || destination == source) return false;
    RenderPathTransform writer = {destination, transform, 0, 0, 0, 0};
    return rdt_path_visit(source, render_path_transform_visit, &writer);
}

static float render_path_point_segment_distance_sq(float point_x, float point_y,
                                                  float start_x, float start_y,
                                                  float end_x, float end_y) {
    float dx = end_x - start_x;
    float dy = end_y - start_y;
    float length_sq = dx * dx + dy * dy;
    if (length_sq <= 0.000001f) {
        float px = point_x - start_x;
        float py = point_y - start_y;
        return px * px + py * py;
    }
    float projection = ((point_x - start_x) * dx + (point_y - start_y) * dy) /
        length_sq;
    projection = clamp_unit(projection);
    float closest_x = start_x + dx * projection;
    float closest_y = start_y + dy * projection;
    float px = point_x - closest_x;
    float py = point_y - closest_y;
    return px * px + py * py;
}

static bool render_path_flatten_cubic_step(float x0, float y0, float x1, float y1,
    float x2, float y2, float x3, float y3, float flatness_sq, int depth,
    RenderPathLineFn line, void* context) {
    if (depth == 0 || (render_path_point_segment_distance_sq(x1, y1, x0, y0, x3, y3) <= flatness_sq &&
        render_path_point_segment_distance_sq(x2, y2, x0, y0, x3, y3) <= flatness_sq))
        return line(context, x0, y0, x3, y3);
    float ax = (x0 + x1) * .5f, ay = (y0 + y1) * .5f;
    float bx = (x1 + x2) * .5f, by = (y1 + y2) * .5f;
    float cx = (x2 + x3) * .5f, cy = (y2 + y3) * .5f;
    float dx = (ax + bx) * .5f, dy = (ay + by) * .5f;
    float ex = (bx + cx) * .5f, ey = (by + cy) * .5f;
    float mx = (dx + ex) * .5f, my = (dy + ey) * .5f;
    return render_path_flatten_cubic_step(x0, y0, ax, ay, dx, dy, mx, my, flatness_sq, depth - 1, line, context) &&
        render_path_flatten_cubic_step(mx, my, ex, ey, cx, cy, x3, y3, flatness_sq, depth - 1, line, context);
}

bool render_path_flatten_cubic(float x0, float y0, float x1, float y1,
    float x2, float y2, float x3, float y3, float flatness, int max_depth,
    RenderPathLineFn line, void* context) {
    if (!line || !isfinite(flatness) || flatness <= 0.0f || max_depth < 0 || max_depth > 20) return false;
    return render_path_flatten_cubic_step(x0, y0, x1, y1, x2, y2, x3, y3,
        flatness * flatness, max_depth, line, context);
}

