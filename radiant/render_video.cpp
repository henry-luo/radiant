// decoded video and controls form an immutable image in the owner's paint order.

#include "render.hpp"
#include "render_metrics.hpp"
#include "rdt_video.h"
#include "event.hpp"
#include "view.hpp"
#include "glyph_sampling.hpp"
#include "lib/font/font.h"
#include "lib/log.h"
#include "lib/mem.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

// ---------------------------------------------------------------------------
// blit_video_frame — scale + clip a video frame onto the target surface
// ---------------------------------------------------------------------------

static void blit_video_frame(ImageSurface* surface, const RdtVideoFrame* frame,
                             float dst_x, float dst_y, float dst_w, float dst_h,
                             const Bound* clip) {
    if (!surface || !surface->pixels || !frame || !frame->pixels) return;
    if (frame->width <= 0 || frame->height <= 0) return;
    if (dst_w <= 0 || dst_h <= 0) return;

    Rect dst_rect = {dst_x, dst_y, dst_w, dst_h};
    Bound local_clip = clip ? *clip : (Bound){0, 0, (float)surface->width, (float)surface->height};
    RasterPaintContext raster = raster_paint_context(surface, &local_clip, nullptr, 0);
    raster_blit_pixels_scaled(&raster, (const uint32_t*)frame->pixels,
                              frame->width, frame->height, frame->stride / 4,
                              &dst_rect, SCALE_MODE_LINEAR);
}

// ---------------------------------------------------------------------------
// video visibility
// ---------------------------------------------------------------------------

// ===========================================================================
// Video controls overlay drawing (ThorVG vector graphics + font system text)
// ===========================================================================

// controls bar constants (in physical pixels)
static const float CONTROLS_HEIGHT = 40.0f;
static const float CONTROLS_PADDING = 8.0f;
static const float PLAY_BTN_SIZE = 24.0f;
static const float SEEK_BAR_HEIGHT = 4.0f;
static const float VOLUME_WIDTH = 60.0f;
static const float VOLUME_HEIGHT = 4.0f;
static const float ICON_MARGIN = 8.0f;

// format time as m:ss
static void format_time(double seconds, char* buf, int bufsize) {
    if (seconds < 0) seconds = 0;
    int total = (int)seconds; // INT_CAST_OK: time formatting
    int m = total / 60;
    int s = total % 60;
    snprintf(buf, bufsize, "%d:%02d", m, s);
}

// blit a grayscale glyph bitmap onto the surface with a given color
static void blit_glyph(ImageSurface* surface, GlyphBitmap* bitmap,
                       int x, int y, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    if (!surface || !bitmap) return;
    Bound clip = {0, 0, (float)surface->width, (float)surface->height};
    Color color = {0};
    color.r = cr;
    color.g = cg;
    color.b = cb;
    color.a = ca;
    glyph_draw_coverage_bitmap(surface, bitmap, x, y, &clip, color);
}

static float video_text_advance(ImageSurface* surface, FontContext* font_ctx,
                                const char* text, float x, float y_baseline,
                                float font_size_px, Color color, bool draw) {
    if (!font_ctx || !text || !text[0]) return 0.0f;
    FontStyleDesc style = font_style_desc_system_ui(font_size_px);
    FontHandle* handle = font_resolve(font_ctx, &style);
    if (!handle) return 0.0f;

    float cx = x;
    for (int i = 0; text[i]; i++) {
        uint32_t codepoint = (uint32_t)(unsigned char)text[i];
        LoadedGlyph* glyph = font_load_glyph(handle, &style, codepoint, draw);
        if (!glyph) { cx += font_size_px * 0.5f; continue; }
        if (draw && glyph->bitmap.pixel_mode == GLYPH_PIXEL_GRAY && glyph->bitmap.buffer) {
            blit_glyph(surface, &glyph->bitmap,
                       (int)(cx + glyph->bitmap.bearing_x),
                       (int)(y_baseline - glyph->bitmap.bearing_y),
                       color.r, color.g, color.b, color.a);
        }
        cx += glyph->advance_x;
    }

    font_handle_release(handle);
    return cx - x;
}

// draw a text string using font system glyphs, returns total advance width
static float draw_font_text(ImageSurface* surface, FontContext* font_ctx,
                            const char* text, float x, float y_baseline,
                            float font_size_px, uint8_t cr, uint8_t cg, uint8_t cb, uint8_t ca) {
    Color color = {};
    color.r = cr;
    color.g = cg;
    color.b = cb;
    color.a = ca;
    return video_text_advance(surface, font_ctx, text, x, y_baseline,
                              font_size_px, color, true);
}

// measure text width without drawing
static float measure_font_text(FontContext* font_ctx, const char* text, float font_size_px) {
    return video_text_advance(nullptr, font_ctx, text, 0.0f, 0.0f,
                              font_size_px, {}, false);
}

// ---------------------------------------------------------------------------
// render_video_controls — draw controls overlay using ThorVG + font text
// ---------------------------------------------------------------------------

static void render_video_controls(ImageSurface* surface, RdtVideo* video,
                                  float dst_x, float dst_y, float dst_w, float dst_h,
                                  UiContext* uicon) {
    if (!surface || !video || dst_w < 120 || dst_h < 60) return;

    RdtVideoState state = rdt_video_get_state(video);

    // create a temporary RdtVector targeting the surface
    RdtVector vec = {};
    int stride = surface->pitch / 4;
    rdt_vector_init(&vec, (uint32_t*)surface->pixels, surface->width, surface->height, stride);

    // controls bar at the bottom
    float bar_h = CONTROLS_HEIGHT;
    float bar_y = dst_y + dst_h - bar_h;
    float bar_x = dst_x;
    float bar_w = dst_w;

    // semi-transparent dark background
    Color bg_color = {0x000000B0};  // ABGR: alpha=B0
    bg_color.r = 0x00; bg_color.g = 0x00; bg_color.b = 0x00; bg_color.a = 0xB0;
    rdt_fill_rounded_rect(&vec, bar_x, bar_y, bar_w, bar_h, 4, 4, bg_color);

    float cx = bar_x + CONTROLS_PADDING;
    float cy_center = bar_y + bar_h / 2.0f;

    // --- Play/Pause button (vector path) ---
    float btn_x = cx;
    float btn_y = cy_center - PLAY_BTN_SIZE / 2.0f;
    Color icon_color = {};
    icon_color.r = 0xFF; icon_color.g = 0xFF; icon_color.b = 0xFF; icon_color.a = 0xE0;

    if (state == RDT_VIDEO_STATE_PLAYING) {
        // pause: two vertical bars
        float bw = PLAY_BTN_SIZE * 0.25f;
        float gap = PLAY_BTN_SIZE * 0.15f;
        float lx = btn_x + PLAY_BTN_SIZE * 0.2f;
        float rx = lx + bw + gap;
        rdt_fill_rounded_rect(&vec, lx, btn_y, bw, PLAY_BTN_SIZE, 1, 1, icon_color);
        rdt_fill_rounded_rect(&vec, rx, btn_y, bw, PLAY_BTN_SIZE, 1, 1, icon_color);
    } else {
        // play: right-pointing triangle
        RdtPath* tri = rdt_path_new();
        float tx = btn_x + PLAY_BTN_SIZE * 0.2f;
        rdt_path_move_to(tri, tx, btn_y);
        rdt_path_line_to(tri, tx + PLAY_BTN_SIZE * 0.7f, cy_center);
        rdt_path_line_to(tri, tx, btn_y + PLAY_BTN_SIZE);
        rdt_path_close(tri);
        rdt_fill_path(&vec, tri, icon_color, RDT_FILL_WINDING, NULL);
        rdt_path_free(tri);
    }
    cx = btn_x + PLAY_BTN_SIZE + ICON_MARGIN;

    // --- Current time (font-rendered text) ---
    double cur_time = rdt_video_get_current_time(video);
    double duration = rdt_video_get_duration(video);
    char time_buf[32];
    format_time(cur_time, time_buf, sizeof(time_buf));
    float font_size = 12.0f;
    FontContext* font_ctx = uicon ? uicon->font_ctx : NULL;
    float text_y_baseline = cy_center + font_size * 0.35f;  // approximate vertical centering
    float tw = draw_font_text(surface, font_ctx, time_buf, cx, text_y_baseline,
                              font_size, 0xFF, 0xFF, 0xFF, 0xD0);
    if (tw < 1) tw = (float)strlen(time_buf) * font_size * 0.6f;
    cx += tw + ICON_MARGIN;

    // --- Seek bar (vector rounded rects) ---
    float seek_x = cx;
    float seek_y = cy_center - SEEK_BAR_HEIGHT / 2.0f;
    char dur_buf[32]; format_time(duration, dur_buf, sizeof(dur_buf));
    float dur_text_w = measure_font_text(font_ctx, dur_buf, font_size);
    if (dur_text_w < 1) dur_text_w = (float)strlen(dur_buf) * font_size * 0.6f;
    float volume_total = 16 + ICON_MARGIN / 2 + VOLUME_WIDTH + ICON_MARGIN;
    float seek_w = bar_w - (cx - bar_x) - ICON_MARGIN - dur_text_w - ICON_MARGIN - volume_total - CONTROLS_PADDING;
    if (seek_w < 40) seek_w = 40;

    // track background
    Color track_color = {};
    track_color.r = 0x80; track_color.g = 0x80; track_color.b = 0x80; track_color.a = 0x80;
    rdt_fill_rounded_rect(&vec, seek_x, seek_y, seek_w, SEEK_BAR_HEIGHT, 2, 2, track_color);

    // progress fill
    float fraction = (duration > 0) ? (float)(cur_time / duration) : 0;
    if (fraction > 1.0f) fraction = 1.0f;
    float prog_w = seek_w * fraction;
    if (prog_w > 1) {
        Color prog_color = {};
        prog_color.r = 0xFF; prog_color.g = 0xFF; prog_color.b = 0xFF; prog_color.a = 0xD0;
        rdt_fill_rounded_rect(&vec, seek_x, seek_y, prog_w, SEEK_BAR_HEIGHT, 2, 2, prog_color);
    }

    // thumb circle
    float thumb_cx = seek_x + prog_w;
    float thumb_r = 5.0f;
    RdtPath* thumb = rdt_path_new();
    rdt_path_add_circle(thumb, thumb_cx, cy_center, thumb_r, thumb_r);
    Color thumb_color = {};
    thumb_color.r = 0xFF; thumb_color.g = 0xFF; thumb_color.b = 0xFF; thumb_color.a = 0xE0;
    rdt_fill_path(&vec, thumb, thumb_color, RDT_FILL_WINDING, NULL);
    rdt_path_free(thumb);

    cx = seek_x + seek_w + ICON_MARGIN;

    // --- Duration text ---
    draw_font_text(surface, font_ctx, dur_buf, cx, text_y_baseline,
                   font_size, 0xFF, 0xFF, 0xFF, 0xD0);
    cx += dur_text_w + ICON_MARGIN;

    // --- Speaker icon (vector path) ---
    float spk_size = 16.0f;
    float spk_x = cx;
    float spk_y = cy_center - spk_size / 2.0f;
    {
        RdtPath* spk = rdt_path_new();
        // speaker body (rectangle)
        float bw = spk_size / 3, bh = spk_size / 2;
        float bx = spk_x, by = spk_y + spk_size / 4;
        rdt_path_add_rect(spk, bx, by, bw, bh, 0, 0);
        rdt_fill_path(&vec, spk, icon_color, RDT_FILL_WINDING, NULL);
        rdt_path_free(spk);

        // speaker cone (triangle)
        RdtPath* cone = rdt_path_new();
        rdt_path_move_to(cone, bx + bw, by);
        rdt_path_line_to(cone, spk_x + spk_size * 2.0f / 3.0f, spk_y);
        rdt_path_line_to(cone, spk_x + spk_size * 2.0f / 3.0f, spk_y + spk_size);
        rdt_path_line_to(cone, bx + bw, by + bh);
        rdt_path_close(cone);
        rdt_fill_path(&vec, cone, icon_color, RDT_FILL_WINDING, NULL);
        rdt_path_free(cone);
    }
    cx += spk_size + ICON_MARGIN / 2;

    // --- Volume slider (vector rounded rects) ---
    float vol_y = cy_center - VOLUME_HEIGHT / 2.0f;
    rdt_fill_rounded_rect(&vec, cx, vol_y, VOLUME_WIDTH, VOLUME_HEIGHT, 2, 2, track_color);

    float vol_level = rdt_video_get_volume(video);
    if (vol_level < 0) vol_level = 0; if (vol_level > 1) vol_level = 1;
    float vol_fill = VOLUME_WIDTH * vol_level;
    if (vol_fill > 1) {
        Color vol_color = {};
        vol_color.r = 0xFF; vol_color.g = 0xFF; vol_color.b = 0xFF; vol_color.a = 0xD0;
        rdt_fill_rounded_rect(&vec, cx, vol_y, vol_fill, VOLUME_HEIGHT, 2, 2, vol_color);
    }

    rdt_vector_destroy(&vec);
}

// ---------------------------------------------------------------------------
// render_error_state — draw error placeholder (X mark)
// ---------------------------------------------------------------------------

static void render_error_state(ImageSurface* surface,
                               float dst_x, float dst_y, float dst_w, float dst_h) {
    RdtVector vec = {};
    int stride = surface->pitch / 4;
    rdt_vector_init(&vec, (uint32_t*)surface->pixels, surface->width, surface->height, stride);

    // dark background
    Color bg = {}; bg.r = 0x20; bg.g = 0x20; bg.b = 0x20; bg.a = 0xFF;
    rdt_fill_rect(&vec, dst_x, dst_y, dst_w, dst_h, bg);

    // X mark
    float ccx = dst_x + dst_w / 2, ccy = dst_y + dst_h / 2;
    float s = fminf(dst_w, dst_h) * 0.15f;
    if (s < 8) s = 8; if (s > 40) s = 40;
    float stroke_w = 3.0f;
    Color x_color = {}; x_color.r = 0xCC; x_color.g = 0x33; x_color.b = 0x33; x_color.a = 0xD0;

    RdtPath* xp = rdt_path_new();
    rdt_path_move_to(xp, ccx - s, ccy - s);
    rdt_path_line_to(xp, ccx + s, ccy + s);
    rdt_stroke_path(&vec, xp, x_color, stroke_w, RDT_CAP_ROUND, RDT_JOIN_ROUND, NULL, 0, 0, NULL);
    rdt_path_free(xp);

    RdtPath* xp2 = rdt_path_new();
    rdt_path_move_to(xp2, ccx + s, ccy - s);
    rdt_path_line_to(xp2, ccx - s, ccy + s);
    rdt_stroke_path(&vec, xp2, x_color, stroke_w, RDT_CAP_ROUND, RDT_JOIN_ROUND, NULL, 0, 0, NULL);
    rdt_path_free(xp2);

    rdt_vector_destroy(&vec);
}

// ---------------------------------------------------------------------------
// render_play_button_overlay — large centered play button for idle/paused video
// ---------------------------------------------------------------------------

static void render_play_button_overlay(ImageSurface* surface,
                                       float dst_x, float dst_y, float dst_w, float dst_h) {
    RdtVector vec = {};
    int stride = surface->pitch / 4;
    rdt_vector_init(&vec, (uint32_t*)surface->pixels, surface->width, surface->height, stride);

    float ccx = dst_x + dst_w / 2, ccy = dst_y + dst_h / 2;
    float btn_r = fminf(dst_w, dst_h) * 0.12f;
    if (btn_r < 16) btn_r = 16; if (btn_r > 40) btn_r = 40;

    // semi-transparent circle background
    RdtPath* circle = rdt_path_new();
    rdt_path_add_circle(circle, ccx, ccy, btn_r, btn_r);
    Color circle_color = {}; circle_color.r = 0x00; circle_color.g = 0x00; circle_color.b = 0x00; circle_color.a = 0x80;
    rdt_fill_path(&vec, circle, circle_color, RDT_FILL_WINDING, NULL);
    rdt_path_free(circle);

    // play triangle inside circle
    float tri_s = btn_r * 0.6f;
    float tx = ccx - tri_s * 0.35f;
    RdtPath* tri = rdt_path_new();
    rdt_path_move_to(tri, tx, ccy - tri_s);
    rdt_path_line_to(tri, tx + tri_s * 1.1f, ccy);
    rdt_path_line_to(tri, tx, ccy + tri_s);
    rdt_path_close(tri);
    Color tri_color = {}; tri_color.r = 0xFF; tri_color.g = 0xFF; tri_color.b = 0xFF; tri_color.a = 0xE0;
    rdt_fill_path(&vec, tri, tri_color, RDT_FILL_WINDING, NULL);
    rdt_path_free(tri);

    rdt_vector_destroy(&vec);
}

ImageSurface* render_video_snapshot(const EmbedProp* embed, bool has_poster,
        float width, float height, float scale, UiContext* ui) {
    RdtVideo* video = embed ? embed->video.get() : nullptr;
    if (!video || width <= 0.0f || height <= 0.0f) return nullptr;
    RdtVideoState state = rdt_video_get_state(video);
    RdtVideoFrame frame = {};
    bool live = (state == RDT_VIDEO_STATE_PLAYING || state == RDT_VIDEO_STATE_PAUSED) &&
        rdt_video_get_frame(video, &frame) == 0 && frame.pixels;
    if (has_poster && !live && !embed->has_controls) return nullptr;
    bool decoded = live || (!has_poster && state != RDT_VIDEO_STATE_ERROR &&
        state != RDT_VIDEO_STATE_IDLE && state != RDT_VIDEO_STATE_LOADING &&
        rdt_video_get_frame(video, &frame) == 0 && frame.pixels);
    if (!has_poster && !decoded && state != RDT_VIDEO_STATE_ERROR &&
        state != RDT_VIDEO_STATE_IDLE && state != RDT_VIDEO_STATE_LOADING) return nullptr;
    MemContext* memory = ui && ui->document ? (MemContext*)ui->document->services.mem_ctx : nullptr;
    ImageSurface* surface = render_surface_create_budgeted(memory, width, height);
    if (!surface) return nullptr;
    surface->alpha_mode = IMAGE_ALPHA_PREMULTIPLIED;
    if (decoded) {
        ImageSurface source = {};
        source.width = frame.width; source.height = frame.height;
        source.has_intrinsic_size = true;
        Rect rect = render_media_object_rect(embed, &source, {0, 0, width, height}, scale);
        blit_video_frame(surface, &frame, rect.x, rect.y, rect.width, rect.height, nullptr);
        if (embed->has_controls) render_video_controls(surface, video, 0, 0, width, height, ui);
        if (state == RDT_VIDEO_STATE_PAUSED || state == RDT_VIDEO_STATE_READY)
            render_play_button_overlay(surface, 0, 0, width, height);
    } else if (!has_poster && state == RDT_VIDEO_STATE_ERROR) {
        render_error_state(surface, 0, 0, width, height);
    } else {
        render_play_button_overlay(surface, 0, 0, width, height);
    }
    // replay workers and retained fragments must not borrow the decoder's mutable frame buffer.
    image_surface_snapshot_publish(surface);
    return surface;
}
