#include "shape_exclusion.hpp"
#include "render.hpp"

#include <float.h>
#include <math.h>

namespace {

struct BandBounds {
    float left = FLT_MAX;
    float right = -FLT_MAX;
    void include(float low, float high) {
        left = fminf(left, low);
        right = fmaxf(right, high);
    }
};

bool valid_band_query(float top, float bottom, float margin, float* left, float* right) {
    return left && right && bottom > top && margin >= 0.0f &&
        isfinite(top) && isfinite(bottom) && isfinite(margin);
}

bool publish_band_bounds(const BandBounds& bounds, float* left, float* right) {
    if (bounds.left > bounds.right || !isfinite(bounds.left) || !isfinite(bounds.right)) return false;
    *left = bounds.left;
    *right = bounds.right;
    return true;
}

float band_distance(float y, float top, float bottom) {
    return fmaxf(fmaxf(top - y, y - bottom), 0.0f);
}

float margin_reach(float y, float top, float bottom, float margin) {
    float distance = band_distance(y, top, bottom);
    return sqrtf(fmaxf(0.0f, margin * margin - distance * distance));
}

template<typename Function>
float concave_maximum(float low, float high, Function value) {
    float result = fmaxf(value(low), value(high));
    // a distance offset of a convex boundary is concave on its feasible band.
    for (int iteration = 0; iteration < 40; iteration++) {
        float a = low + (high - low) / 3.0f;
        float b = high - (high - low) / 3.0f;
        float va = value(a), vb = value(b);
        result = fmaxf(result, fmaxf(va, vb));
        if (va < vb) low = a;
        else high = b;
    }
    return result;
}

bool band_intersects(float low, float high, float top, float bottom, float margin) {
    return high + margin > top && low - margin < bottom;
}

void ellipse_band(BandBounds* bounds, float cx, float cy, float rx, float ry,
        float top, float bottom, float margin) {
    if (rx < 0.0f || ry < 0.0f ||
        !band_intersects(cy - ry, cy + ry, top, bottom, margin) ||
        (ry == 0.0f && margin == 0.0f)) return;
    if (rx == ry) {
        float radius = rx + margin;
        float distance = band_distance(cy, top, bottom);
        float reach = sqrtf(fmaxf(0.0f, radius * radius - distance * distance));
        bounds->include(cx - reach, cx + reach);
        return;
    }
    float low = fmaxf(cy - ry, top - margin);
    float high = fminf(cy + ry, bottom + margin);
    float reach = concave_maximum(low, high, [&](float y) {
        float normalized = ry > 0.0f ? (y - cy) / ry : 0.0f;
        return rx * sqrtf(fmaxf(0.0f, 1.0f - normalized * normalized)) +
            margin_reach(y, top, bottom, margin);
    });
    bounds->include(cx - reach, cx + reach);
}

void segment_band(BandBounds* bounds, float x0, float y0, float x1, float y1,
        float top, float bottom, float margin) {
    if (!band_intersects(fminf(y0, y1), fmaxf(y0, y1), top, bottom, margin)) return;
    if (y0 > y1) {
        float saved = y0; y0 = y1; y1 = saved;
        saved = x0; x0 = x1; x1 = saved;
    }
    if (y0 == y1) {
        float reach = margin_reach(y0, top, bottom, margin);
        bounds->include(fminf(x0, x1) - reach, fmaxf(x0, x1) + reach);
        return;
    }
    float low = fmaxf(y0, top - margin), high = fminf(y1, bottom + margin);
    auto x_at = [&](float y) { return x0 + (x1 - x0) * ((y - y0) / (y1 - y0)); };
    float right = concave_maximum(low, high, [&](float y) {
        return x_at(y) + margin_reach(y, top, bottom, margin);
    });
    float negative_left = concave_maximum(low, high, [&](float y) {
        return -x_at(y) + margin_reach(y, top, bottom, margin);
    });
    bounds->include(-negative_left, right);
}

void rectangle_band(BandBounds* bounds, float x, float y, float width, float height,
        float top, float bottom, float margin) {
    if (width < 0.0f || height < 0.0f ||
        (height == 0.0f && margin == 0.0f) ||
        !band_intersects(y, y + height, top, bottom, margin)) return;
    float distance = fmaxf(fmaxf(y - bottom, top - y - height), 0.0f);
    float reach = sqrtf(fmaxf(0.0f, margin * margin - distance * distance));
    bounds->include(x - reach, x + width + reach);
}

void rounded_box_band(BandBounds* bounds, const Rect& rect, const Corner* input_radius,
        float top, float bottom, float margin) {
    if (rect.height == 0.0f || !input_radius) {
        rectangle_band(bounds, rect.x, rect.y, rect.width, rect.height, top, bottom, margin);
        return;
    }
    if (rect.width < 0.0f || rect.height < 0.0f ||
        !band_intersects(rect.y, rect.y + rect.height, top, bottom, margin)) return;
    Corner radius = *input_radius;
    constrain_corner_radii(&radius, rect.width, rect.height);
    float low = fmaxf(rect.y, top - margin);
    float high = fminf(rect.y + rect.height, bottom + margin);
    for (int side = 0; side < 2; side++) {
        int lower_corner = side ? 2 : 3;
        float upper = radius.vertical[side], lower = radius.vertical[lower_corner];
        float upper_x = radius.horizontal[side], lower_x = radius.horizontal[lower_corner];
        float edge = side ? rect.x + rect.width : -rect.x;
        float extent = concave_maximum(low, high, [&](float y) {
            float inset = 0.0f;
            if (upper > 0.0f && y < rect.y + upper) {
                float dy = (y - rect.y - upper) / upper;
                inset = upper_x * (1.0f - sqrtf(fmaxf(0.0f, 1.0f - dy * dy)));
            } else if (lower > 0.0f && y > rect.y + rect.height - lower) {
                float dy = (y - rect.y - rect.height + lower) / lower;
                inset = lower_x * (1.0f - sqrtf(fmaxf(0.0f, 1.0f - dy * dy)));
            }
            return edge - inset + margin_reach(y, top, bottom, margin);
        });
        if (side) bounds->right = extent;
        else bounds->left = -extent;
    }
}

} // namespace

bool shape_exclusion_band(const ClipShape* shape, float top, float bottom,
        float margin, float* left, float* right) {
    if (!shape || !valid_band_query(top, bottom, margin, left, right) || shape->transformed) return false;
    BandBounds bounds;
    switch (shape->type) {
        case CLIP_SHAPE_CIRCLE:
            ellipse_band(&bounds, shape->circle.cx, shape->circle.cy,
                shape->circle.r, shape->circle.r, top, bottom, margin);
            break;
        case CLIP_SHAPE_ELLIPSE:
            ellipse_band(&bounds, shape->ellipse.cx, shape->ellipse.cy,
                shape->ellipse.rx, shape->ellipse.ry, top, bottom, margin);
            break;
        case CLIP_SHAPE_INSET:
            rectangle_band(&bounds, shape->inset.x, shape->inset.y,
                shape->inset.w, shape->inset.h, top, bottom, margin);
            break;
        case CLIP_SHAPE_ROUNDED_RECT: {
            const auto& r = shape->rounded_rect;
            Corner radius = {};
            radius.top_left = radius.top_left_y = r.r_tl;
            radius.top_right = radius.top_right_y = r.r_tr;
            radius.bottom_right = radius.bottom_right_y = r.r_br;
            radius.bottom_left = radius.bottom_left_y = r.r_bl;
            rounded_box_band(&bounds, {r.x, r.y, r.w, r.h}, &radius, top, bottom, margin);
            break;
        }
        case CLIP_SHAPE_POLYGON: {
            const auto& polygon = shape->polygon;
            if (polygon.count < 3 || !polygon.vx || !polygon.vy) return false;
            for (int i = 0, j = polygon.count - 1; i < polygon.count; j = i++) {
                segment_band(&bounds, polygon.vx[j], polygon.vy[j], polygon.vx[i], polygon.vy[i],
                    top, bottom, margin);
            }
            break;
        }
        default: return false;
    }
    return publish_band_bounds(bounds, left, right);
}

bool shape_exclusion_box_band(const Rect& rect, const Corner* radius,
        float top, float bottom, float margin, float* left, float* right) {
    if (!valid_band_query(top, bottom, margin, left, right)) return false;
    BandBounds bounds;
    rounded_box_band(&bounds, rect, radius, top, bottom, margin);
    return publish_band_bounds(bounds, left, right);
}

bool shape_exclusion_alpha_rows(const ImagePixelView* image, float threshold,
        ShapeImageRow* rows, size_t capacity) {
    if (!image || !rows || !image->pixels || !image->width || !image->height ||
        capacity < image->height || !isfinite(threshold) ||
        (image->channels != 1 && image->channels != 3 && image->channels != 4) ||
        image->width > SIZE_MAX / image->channels) return false;
    size_t row_bytes = (size_t)image->width * image->channels;
    if (image->stride < row_bytes || image->capacity < row_bytes ||
        image->height - 1 > (image->capacity - row_bytes) / image->stride) return false;
    threshold = fminf(fmaxf(threshold, 0.0f), 1.0f);
    float width = (float)image->width;
    for (size_t source_y = 0; source_y < image->height; source_y++) {
        const uint8_t* source = image->pixels + source_y * image->stride;
        ShapeImageRow row = {1.0f, 0.0f};
        for (uint32_t source_x = 0; source_x < image->width; source_x++) {
            float alpha = image->channels == 4
                ? source[(size_t)source_x * 4 + 3] / 255.0f : 1.0f;
            if (!(alpha > threshold)) continue;
            float start = (float)source_x / width;
            float end = (float)(source_x + 1) / width;
            row.left = fminf(row.left, start);
            row.right = fmaxf(row.right, end);
        }
        rows[source_y] = row;
    }
    return true;
}

bool shape_exclusion_image_band(const ShapeImageRow* rows, size_t count,
        const Rect& image_rect, float top, float bottom, float margin, float* left, float* right) {
    if (!rows || !count || !valid_band_query(top, bottom, margin, left, right) ||
        !isfinite(image_rect.x) || !isfinite(image_rect.y) ||
        !isfinite(image_rect.width) || !isfinite(image_rect.height) ||
        !(image_rect.width > 0.0f) || !(image_rect.height > 0.0f)) return false;
    BandBounds bounds;
    float row_height = image_rect.height / (float)count;
    for (size_t source_y = 0; source_y < count; source_y++) {
        const ShapeImageRow& row = rows[source_y];
        if (row.left > row.right) continue;
        if (!isfinite(row.left) || !isfinite(row.right) || row.left < 0.0f || row.right > 1.0f)
            return false;
        rectangle_band(&bounds, image_rect.x + row.left * image_rect.width,
            image_rect.y + (float)source_y * row_height,
            (row.right - row.left) * image_rect.width, row_height, top, bottom, margin);
    }
    return publish_band_bounds(bounds, left, right);
}
