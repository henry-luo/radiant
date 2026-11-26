#include "render.hpp"

#include "lib/math_checked.hpp"
#include "lib/math_utils.h"
#include <math.h>
#include <string.h>

// --- CSS Blend Mode Functions (CSS Compositing and Blending Level 1) ---
// All operate on normalized [0,1] channel values.
static inline float render_blend_multiply(float Cb, float Cs) { return Cb * Cs; }
static inline float render_blend_screen(float Cb, float Cs) { return Cb + Cs - Cb * Cs; }
static inline float render_blend_overlay(float Cb, float Cs) {
    return Cb <= 0.5f ? 2.0f * Cb * Cs : 1.0f - 2.0f * (1.0f - Cb) * (1.0f - Cs);
}
static inline float render_blend_darken(float Cb, float Cs) { return Cb < Cs ? Cb : Cs; }
static inline float render_blend_lighten(float Cb, float Cs) { return Cb > Cs ? Cb : Cs; }
static inline float render_blend_color_dodge(float Cb, float Cs) {
    if (Cb <= 0.0f) return 0.0f;
    if (Cs >= 1.0f) return 1.0f;
    float value = Cb / (1.0f - Cs);
    return value < 1.0f ? value : 1.0f;
}
static inline float render_blend_color_burn(float Cb, float Cs) {
    if (Cb >= 1.0f) return 1.0f;
    if (Cs <= 0.0f) return 0.0f;
    float value = 1.0f - (1.0f - Cb) / Cs;
    return value > 0.0f ? value : 0.0f;
}
static inline float render_blend_hard_light(float Cb, float Cs) {
    return Cs <= 0.5f ? 2.0f * Cb * Cs : 1.0f - 2.0f * (1.0f - Cb) * (1.0f - Cs);
}
static inline float render_blend_soft_light(float Cb, float Cs) {
    if (Cs <= 0.5f) return Cb - (1.0f - 2.0f * Cs) * Cb * (1.0f - Cb);
    float D = Cb <= 0.25f ? ((16.0f * Cb - 12.0f) * Cb + 4.0f) * Cb : sqrtf(Cb);
    return Cb + (2.0f * Cs - 1.0f) * (D - Cb);
}
static inline float render_blend_difference(float Cb, float Cs) { return fabsf(Cb - Cs); }
static inline float render_blend_exclusion(float Cb, float Cs) { return Cb + Cs - 2.0f * Cb * Cs; }

static inline uint8_t render_composite_blend_channel(uint8_t Cb_byte, uint8_t Cs_byte, CssEnum mode) {
    float Cb = Cb_byte / 255.0f;
    float Cs = Cs_byte / 255.0f;
    float result;
    switch (mode) {
        case CSS_VALUE_MULTIPLY:    result = render_blend_multiply(Cb, Cs); break;
        case CSS_VALUE_SCREEN:      result = render_blend_screen(Cb, Cs); break;
        case CSS_VALUE_OVERLAY:     result = render_blend_overlay(Cb, Cs); break;
        case CSS_VALUE_DARKEN:      result = render_blend_darken(Cb, Cs); break;
        case CSS_VALUE_LIGHTEN:     result = render_blend_lighten(Cb, Cs); break;
        case CSS_VALUE_COLOR_DODGE: result = render_blend_color_dodge(Cb, Cs); break;
        case CSS_VALUE_COLOR_BURN:  result = render_blend_color_burn(Cb, Cs); break;
        case CSS_VALUE_HARD_LIGHT:  result = render_blend_hard_light(Cb, Cs); break;
        case CSS_VALUE_SOFT_LIGHT:  result = render_blend_soft_light(Cb, Cs); break;
        case CSS_VALUE_DIFFERENCE:  result = render_blend_difference(Cb, Cs); break;
        case CSS_VALUE_EXCLUSION:   result = render_blend_exclusion(Cb, Cs); break;
        default:                    result = Cs; break;
    }
    int value = (int)(result * 255.0f + 0.5f);
    return clamp_byte(value);
}

static float render_blend_luminosity(const float color[3]) {
    return .3f * color[0] + .59f * color[1] + .11f * color[2];
}

static float render_blend_saturation(const float color[3]) {
    return fmaxf(color[0], fmaxf(color[1], color[2])) - fminf(color[0], fminf(color[1], color[2]));
}

static void render_blend_set_luminosity(float color[3], float target) {
    float shift = target - render_blend_luminosity(color);
    for (unsigned channel = 0; channel < 3; channel++) color[channel] += shift;
    float low = fminf(color[0], fminf(color[1], color[2])), high = fmaxf(color[0], fmaxf(color[1], color[2]));
    // clip all channels around luminosity so gamut clipping preserves the selected component.
    for (unsigned channel = 0; channel < 3; channel++) {
        if (low < 0.0f) color[channel] = target + (color[channel] - target) * target / (target - low);
        if (high > 1.0f) color[channel] = target + (color[channel] - target) * (1.0f - target) / (high - target);
    }
}

static void render_blend_set_saturation(float color[3], float target) {
    unsigned order[3] = {0, 1, 2};
    for (unsigned index = 1; index < 3; index++) for (unsigned prior = index; prior && color[order[prior - 1]] > color[order[prior]]; prior--) {
        unsigned swap = order[prior]; order[prior] = order[prior - 1]; order[prior - 1] = swap;
    }
    float range = color[order[2]] - color[order[0]];
    color[order[1]] = range > 0.0f ? (color[order[1]] - color[order[0]]) * target / range : 0.0f;
    color[order[2]] = range > 0.0f ? target : 0.0f; color[order[0]] = 0.0f;
}

void render_composite_blend_rgb(uint32_t backdrop, uint32_t source, CssEnum mode, float result[3]) {
    float background[3], foreground[3];
    for (unsigned channel = 0; channel < 3; channel++) {
        background[channel] = (float)((backdrop >> (channel * 8)) & 255u) / 255.0f;
        foreground[channel] = (float)((source >> (channel * 8)) & 255u) / 255.0f;
        result[channel] = (mode == CSS_VALUE_SATURATION_BLEND || mode == CSS_VALUE_LUMINOSITY_BLEND) ? background[channel] : foreground[channel];
    }
    switch (mode) {
    case CSS_VALUE_HUE_BLEND: render_blend_set_saturation(result, render_blend_saturation(background)); break;
    case CSS_VALUE_SATURATION_BLEND: render_blend_set_saturation(result, render_blend_saturation(foreground)); break;
    case CSS_VALUE_COLOR_BLEND: case CSS_VALUE_LUMINOSITY_BLEND: break;
    default:
        for (unsigned channel = 0; channel < 3; channel++) result[channel] = (float)render_composite_blend_channel(
            (uint8_t)(backdrop >> (channel * 8)), (uint8_t)(source >> (channel * 8)), mode) / 255.0f;
        return;
    }
    render_blend_set_luminosity(result, render_blend_luminosity(mode == CSS_VALUE_LUMINOSITY_BLEND ? foreground : background));
}

uint32_t render_composite_blend_pixel(uint32_t backdrop, uint32_t source, CssEnum blend_mode) {
    uint8_t sa = (source >> 24) & 0xFF;
    if (sa == 0) return backdrop;
    uint8_t ba = (backdrop >> 24) & 0xFF;
    if (ba == 0) return source;

    uint8_t sr = source & 0xFF, sg = (source >> 8) & 0xFF, sb = (source >> 16) & 0xFF;
    uint8_t br = backdrop & 0xFF, bg = (backdrop >> 8) & 0xFF, bb = (backdrop >> 16) & 0xFF;

    float mixed[3]; render_composite_blend_rgb(backdrop, source, blend_mode, mixed);
    if (sa == 255 && ba == 255) {
        uint8_t rr = clamp_byte_round(mixed[0] * 255.0f);
        uint8_t rg = clamp_byte_round(mixed[1] * 255.0f);
        uint8_t rb = clamp_byte_round(mixed[2] * 255.0f);
        return render_pixel_pack_abgr(rr, rg, rb, 255u);
    }

    float fa = ba / 255.0f;
    float fsa = sa / 255.0f;
    float ra = fa + fsa - fa * fsa;
    if (ra < 0.001f) return 0;

    float p = (1.0f - fa) * fsa;
    float q = (1.0f - fsa) * fa;
    float t = fa * fsa;
    auto blendch = [&](uint8_t Cb_b, uint8_t Cs_b, unsigned channel) -> uint8_t {
        float Bb = mixed[channel];
        float Co = (p * (Cs_b / 255.0f) + q * (Cb_b / 255.0f) + t * Bb) / ra;
        int value = (int)(Co * 255.0f + 0.5f);
        return clamp_byte(value);
    };
    uint8_t rr = blendch(br, sr, 0);
    uint8_t rg = blendch(bg, sg, 1);
    uint8_t rb = blendch(bb, sb, 2);
    uint8_t new_a = clamp_byte_round(ra * 255.0f);
    return render_pixel_pack_abgr(rr, rg, rb, new_a);
}

struct MaskLayerSampling {
    const ImageSurface* image;
    int width, height;
    BackgroundTilePlan tiles;
    Rect clip;
    MaskLayerOptions options;
    const RdtGradientStop* stops;
    int stop_count;
    GradientType gradient;
    RadiantGradientLine line;
    RadiantRadialGeometry radial;
    RadiantGradientCenter center;
    float angle;
    float first_stop;
    float period;
    bool solid_gradient;
    float solid_alpha;
};

static Rect mask_box(const CssMaskPaint& mask, CssEnum box) {
    if (box == CSS_VALUE_CONTENT_BOX) return mask.content_box;
    if (box == CSS_VALUE_PADDING_BOX) return mask.padding_box;
    return mask.border_box;
}

static float mask_color_alpha(uint32_t color, bool luminance, bool premultiplied) {
    float alpha = (color >> 24) / 255.0f;
    if (!luminance) return alpha;
    float luma = (0.2125f * (color & 255u) + 0.7154f * ((color >> 8) & 255u) +
        0.0721f * ((color >> 16) & 255u)) / 255.0f;
    return premultiplied ? luma : luma * alpha;
}

struct MaskGradientSource {
    const GradientStop* stops;
    int count;
    bool repeat;
};

static MaskGradientSource mask_gradient_source(const BackgroundLayer& layer) {
    if (layer.linear_gradient) return {layer.linear_gradient->stops.get(), layer.linear_gradient->stop_count, layer.linear_gradient->is_repeating};
    if (layer.radial_gradient) return {layer.radial_gradient->stops.get(), layer.radial_gradient->stop_count, layer.radial_gradient->is_repeating};
    if (layer.conic_gradient) return {layer.conic_gradient->stops.get(), layer.conic_gradient->stop_count, layer.conic_gradient->is_repeating};
    return {};
}

static void prepare_mask_gradient(MaskLayerSampling* sample, const BackgroundLayer& layer,
        MaskGradientSource gradient, RdtGradientStop* stops) {
    if (!gradient.stops || gradient.count < 2) return;
    const Rect tile = {0, 0, sample->tiles.tile_w, sample->tiles.tile_h};
    float length = 1.0f;
    if (layer.linear_gradient) {
        sample->gradient = GRADIENT_LINEAR;
        sample->line = radiant_linear_gradient_line(tile, radiant_linear_gradient_used_angle(layer.linear_gradient, tile));
        length = hypotf(sample->line.x2 - sample->line.x1, sample->line.y2 - sample->line.y1);
    } else if (layer.radial_gradient) {
        sample->gradient = GRADIENT_RADIAL;
        sample->radial = radiant_radial_gradient_geometry(layer.radial_gradient, tile);
        length = sample->radial.rx;
    } else {
        sample->gradient = GRADIENT_CONIC;
        sample->center = radiant_gradient_center(layer.conic_gradient.get(), tile);
        sample->angle = math_degrees_to_radians(layer.conic_gradient->from_angle - 90.0f);
    }
    if (!render_copy_gradient_stops(gradient.stops, gradient.count, length, stops, gradient.count, &sample->stop_count)) return;
    sample->stops = stops;
    sample->first_stop = stops[0].offset;
    sample->period = gradient.repeat ? stops[sample->stop_count - 1].offset - sample->first_stop : 0.0f;
    Color first = render_gradient_stop_color(stops[0]);
    sample->solid_gradient = true;
    for (int index = 1; index < sample->stop_count; ++index) {
        if (render_gradient_stop_color(stops[index]).c != first.c) { sample->solid_gradient = false; break; }
    }
    if (gradient.repeat && sample->period <= 0.0f) {
        first = render_gradient_average_color(stops, sample->stop_count);
        sample->solid_gradient = true;
    }
    if (sample->solid_gradient) sample->solid_alpha = mask_color_alpha(first.c, sample->options.luminance, false);
}

static MaskLayerSampling prepare_mask_layer(const CssMaskPaint& mask, int index, RdtGradientStop* stops) {
    MaskLayerSampling sample = {};
    const BackgroundLayer& layer = mask.style->image(index);
    sample.options = mask.style->options(index);
    const ImageSurface* image = image_surface_lookup(sample.options.prepared_image);
    if (image && image->pixels) {
        sample.width = image->decoded_width > 0 ? image->decoded_width : image->width;
        sample.height = image->decoded_height > 0 ? image->decoded_height : image->height;
    }
    bool bitmap = sample.width > 0 && sample.height > 0;
    MaskGradientSource gradient = mask_gradient_source(layer);
    if (!bitmap && (!gradient.stops || !stops)) return sample;
    Rect origin = mask_box(mask, layer.bg_origin);
    sample.clip = mask_box(mask, layer.bg_clip);
    float density = bitmap ? layout_css_image_density(image, layer.image_resolution) : 1.0f;
    if (!compute_background_tile_plan(&layer, origin, sample.options.no_clip ? origin : sample.clip,
            bitmap ? image->width / density : origin.width, bitmap ? image->height / density : origin.height,
            &sample.tiles, 1.0f, bitmap)) return sample;
    // spaced repeats are anchored at the origin once more than one tile fits.
    if (sample.tiles.repeat_x == CSS_VALUE_SPACE && sample.tiles.end_col > 0) sample.tiles.origin_x = origin.x;
    if (sample.tiles.repeat_y == CSS_VALUE_SPACE && sample.tiles.end_row > 0) sample.tiles.origin_y = origin.y;
    if (bitmap) sample.image = image;
    else prepare_mask_gradient(&sample, layer, gradient, stops);
    return sample;
}

static float mask_tile_coordinate(float coordinate, float origin, float extent,
                                  float gap, CssEnum repeat, int last_tile) {
    float offset = coordinate - origin;
    if (repeat == CSS_VALUE_SPACE && last_tile > 0) {
        float step = extent + gap;
        float tile = floorf(offset / step);
        if (tile < 0.0f || tile > last_tile) return -1.0f;
        offset -= tile * step;
    } else if (repeat != CSS_VALUE_NO_REPEAT && repeat != CSS_VALUE_SPACE) {
        offset -= floorf(offset / extent) * extent;
    }
    return offset >= 0.0f && offset < extent ? offset / extent : -1.0f;
}

static float mask_gradient_position(const MaskLayerSampling& sample, float x, float y) {
    if (sample.gradient == GRADIENT_LINEAR) {
        const RadiantGradientLine& line = sample.line;
        float dx = line.x2 - line.x1, dy = line.y2 - line.y1;
        float length_squared = dx * dx + dy * dy;
        return length_squared > 0.0f ? ((x - line.x1) * dx + (y - line.y1) * dy) / length_squared : 1.0f;
    }
    if (sample.gradient == GRADIENT_CONIC) return math_wrap_positive_f(
        atan2f(y - sample.center.y, x - sample.center.x) - sample.angle, math_tau_f()) / math_tau_f();
    const RadiantRadialGeometry& radial = sample.radial;
    return radial.rx > 0.0f && radial.ry > 0.0f
        ? hypotf((x - radial.cx) / radial.rx, (y - radial.cy) / radial.ry)
        : sample.stops[sample.stop_count - 1].offset;
}

static float mask_layer_alpha(const MaskLayerSampling& sample, float x, float y) {
    if (!sample.image && !sample.stops) return 0.0f;
    const Rect& clip = sample.clip;
    if (!sample.options.no_clip &&
        (x < clip.x || y < clip.y || x >= clip.x + clip.width || y >= clip.y + clip.height)) return 0.0f;
    const BackgroundTilePlan& tiles = sample.tiles;
    float u = mask_tile_coordinate(x, tiles.origin_x, tiles.tile_w, tiles.space_gap_x, tiles.repeat_x, tiles.end_col);
    float v = mask_tile_coordinate(y, tiles.origin_y, tiles.tile_h, tiles.space_gap_y, tiles.repeat_y, tiles.end_row);
    if (u < 0.0f || v < 0.0f) return 0.0f;
    bool luminance = sample.options.luminance;
    if (sample.image) {
        // convert a straight source before filtering so luminance includes alpha exactly once.
        uint32_t pixel = render_pixel_sample_bilinear((const uint8_t*)sample.image->pixels,
            sample.width, sample.height, sample.image->pitch, u * sample.width - 0.5f, v * sample.height - 0.5f,
            false, true, luminance && sample.image->alpha_mode == IMAGE_ALPHA_STRAIGHT);
        return mask_color_alpha(pixel, luminance, true);
    }
    if (sample.solid_gradient) return sample.solid_alpha;
    float position = mask_gradient_position(sample, u * tiles.tile_w, v * tiles.tile_h);
    if (sample.period > 0.0f)
        position = sample.first_stop + math_wrap_positive_f(position - sample.first_stop, sample.period);
    Color color = render_sample_gradient_stops(sample.stops, sample.stop_count, position, 1.0f);
    return mask_color_alpha(color.c, luminance, false);
}

static float composite_mask_alpha(const CssMaskPaint& mask, const MaskLayerSampling* layers, float x, float y) {
    if (!layers || !rdt_matrix_project_point(&mask.inverse, x, y, &x, &y)) return 0.0f;
    int bottom = mask.style->image_count - 1;
    float alpha = mask_layer_alpha(layers[bottom], x, y);
    // the last layer has no destination. Each layer above it sees the isolated result so far.
    for (int index = bottom - 1; index >= 0; --index) {
        float source = mask_layer_alpha(layers[index], x, y);
        switch (layers[index].options.composite) {
            case CssMaskComposite::Add: alpha = source + alpha * (1.0f - source); break;
            case CssMaskComposite::Subtract: alpha = source * (1.0f - alpha); break;
            case CssMaskComposite::Intersect: alpha *= source; break;
            case CssMaskComposite::Exclude: alpha = source * (1.0f - alpha) + alpha * (1.0f - source); break;
        }
    }
    return alpha;
}

void render_composite_apply_region(ImageSurface* surface, const uint32_t* backdrop,
                                   int x0, int y0, int width, int height,
                                   RenderCompositeRegionMode mode,
                                   CssEnum blend_mode, uint8_t opacity,
                                   const ClipShape* exclude_shape,
                                   const ClipShape* include_shape,
                                   const CssMaskPaint* mask, float offset_x, float offset_y) {
    if (!surface || !surface->pixels || !backdrop || width <= 0 || height <= 0) return;
    ImageSurfaceReadScope mask_reads;
    bool masked = mask && mask->active();
    lam::Temp<MaskLayerSampling> layers;
    lam::Temp<RdtGradientStop> stops;
    if (masked && mask->invertible) {
        const MaskProp& style = *mask->style;
        size_t total = 0;
        bool valid = true;
        for (int index = 0; valid && index < style.image_count; ++index) {
            int count = mask_gradient_source(style.image(index)).count;
            valid = count >= 0 && math_checked_add(total, (size_t)count, &total);
        }
        if (valid && total) stops = lam::temp_array<RdtGradientStop>(total, MEM_CAT_RENDER);
        if (valid) layers = lam::temp_array<MaskLayerSampling>((size_t)style.image_count, MEM_CAT_RENDER);
        if (layers) {
            size_t start = 0;
            for (int index = 0; index < style.image_count; ++index) {
                int count = mask_gradient_source(style.image(index)).count;
                layers.get()[index] = prepare_mask_layer(*mask, index, stops && count ? stops.get() + start : nullptr);
                start += (size_t)count;
            }
        }
    }
    uint32_t* pixels = (uint32_t*)surface->pixels;
    int pitch = surface->pitch / 4;
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            if (exclude_shape || include_shape) {
                float px = (float)(x0 + col) + 0.5f;
                float py = (float)(y0 + row) + 0.5f;
                if ((exclude_shape && clip_point_in_shape(
                        const_cast<ClipShape*>(exclude_shape), px, py)) ||
                    (include_shape && !clip_point_in_shape(
                        const_cast<ClipShape*>(include_shape), px, py))) continue;
            }
            uint32_t* pixel = &pixels[(y0 + row) * pitch + (x0 + col)];
            uint32_t source = backdrop[row * width + col];
            uint8_t coverage = opacity;
            if (masked) {
                float x = x0 + col + offset_x + 0.5f;
                float y = y0 + row + offset_y + 0.5f;
                coverage = clamp_byte_round(opacity * composite_mask_alpha(*mask, layers.get(), x, y));
            }
            if (mode == RENDER_COMPOSITE_REGION_BLEND) {
                // blend functions use straight channels; native opacity groups retain premultiplied pixels.
                bool premultiplied = surface->alpha_mode == IMAGE_ALPHA_PREMULTIPLIED;
                uint32_t result = render_composite_blend_pixel(
                    premultiplied ? render_pixel_unpremultiply_abgr(source) : source,
                    premultiplied ? render_pixel_unpremultiply_abgr(*pixel) : *pixel, blend_mode);
                *pixel = premultiplied ? render_pixel_premultiply_abgr(result) : result;
            } else if (mode == RENDER_COMPOSITE_REGION_PREMULTIPLIED) {
                // full coverage already has the required premultiplied representation.
                *pixel = render_pixel_source_over_premultiplied_pair(source,
                    coverage == 255 ? *pixel : render_pixel_scale_premultiplied(*pixel, coverage));
            } else if (mode == RENDER_COMPOSITE_REGION_PREMULTIPLIED_FULL) {
                // snapshots unpremultiply after replay; shadows must retain the destination's representation.
                *pixel = surface->alpha_mode == IMAGE_ALPHA_PREMULTIPLIED
                    ? render_pixel_source_over_premultiplied_pair(*pixel, source)
                    : render_pixel_source_over_premultiplied(*pixel, source);
            } else {
                *pixel = render_pixel_source_over_straight(
                    source, *pixel, coverage);
            }
        }
    }
}

void render_composite_blend_surface(ImageSurface* surface, const uint32_t* backdrop,
                                    int x0, int y0, int width, int height,
                                    CssEnum blend_mode) {
    render_composite_apply_region(surface, backdrop, x0, y0, width, height,
                                  RENDER_COMPOSITE_REGION_BLEND, blend_mode, 255);
}

void render_composite_source_over_premul(ImageSurface* surface, const uint32_t* backdrop,
                                         int x0, int y0, int width, int height, uint8_t opacity) {
    render_composite_apply_region(surface, backdrop, x0, y0, width, height,
                                  RENDER_COMPOSITE_REGION_PREMULTIPLIED,
                                  CSS_VALUE_NORMAL, opacity);
}

void render_composite_opacity(ImageSurface* surface, const uint32_t* backdrop,
                              int x0, int y0, int width, int height,
                              float opacity) {
    int opacity_i = (int)(opacity * 255.0f + 0.5f);
    render_composite_apply_region(surface, backdrop, x0, y0, width, height,
                                  RENDER_COMPOSITE_REGION_OPACITY, CSS_VALUE_NORMAL,
                                  clamp_byte(opacity_i));
}
