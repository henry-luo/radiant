// Radiant paint engine kernels (Radiant_Design_Paint_Engine PE3, PE4, PE7, PE11).
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "radiant/paint/paint.hpp"

namespace {

// a fully allocated tile set over plain heap buffers, as the Lambda bridge supplies them.
struct TestLayer {
    PaintTiles tiles = {};
    PaintView* views = nullptr;
    TestLayer(int32_t width, int32_t height, int32_t tile, PaintFormat format, PaintElem elem, bool allocate = true,
              int32_t bands = 0) {
        tiles.width = width; tiles.height = height; tiles.tile_size = tile;
        tiles.tiles_x = (width + tile - 1) / tile; tiles.tiles_y = (height + tile - 1) / tile;
        tiles.format = format; tiles.elem = elem; tiles.bands = bands;
        int32_t channels = format == PAINT_FORMAT_RGBA ? 4 : format == PAINT_FORMAT_PIGMENT ? paint_pigment_channels(bands) : 1;
        size_t sample = elem == PAINT_ELEM_F32 ? sizeof(float) : 1;
        views = (PaintView*)calloc((size_t)tiles.tiles_x * tiles.tiles_y, sizeof(PaintView));
        for (int32_t i = 0; i < tiles.tiles_x * tiles.tiles_y; i++) {
            views[i].elem = elem; views[i].width = tile; views[i].height = tile; views[i].channels = channels;
            views[i].pixel_stride = channels; views[i].row_stride = (int64_t)tile * channels;
            views[i].data = allocate ? calloc((size_t)tile * tile * channels, sample) : nullptr;
        }
        tiles.tiles = views;
    }
    ~TestLayer() {
        for (int32_t i = 0; i < tiles.tiles_x * tiles.tiles_y; i++) free(views[i].data);
        free(views);
    }
    float* rgba(int32_t x, int32_t y) {
        PaintView& v = views[(y / tiles.tile_size) * tiles.tiles_x + x / tiles.tile_size];
        return (float*)v.data + (y % tiles.tile_size) * v.row_stride + (x % tiles.tile_size) * v.pixel_stride;
    }
    float* pigment(int32_t x, int32_t y) { return rgba(x, y); }   // a pixel's 3B + 2 fields
    float coverage(int32_t x, int32_t y) {
        PaintView& v = views[(y / tiles.tile_size) * tiles.tiles_x + x / tiles.tile_size];
        int64_t at = (y % tiles.tile_size) * v.row_stride + (x % tiles.tile_size) * v.pixel_stride;
        return tiles.elem == PAINT_ELEM_F32 ? ((float*)v.data)[at] : ((uint8_t*)v.data)[at] / 255.0f;
    }
};

// a neutral v1 dab row: round, opaque black, no unsupported feature in use.
void dab_row(float* row, float x, float y, float radius) {
    for (int c = 0; c < PAINT_DAB_COLUMNS; c++) row[c] = 0.0f;
    row[PAINT_DAB_X] = x; row[PAINT_DAB_Y] = y; row[PAINT_DAB_RADIUS] = radius;
    row[PAINT_DAB_ASPECT] = 1.0f; row[PAINT_DAB_HARDNESS] = 1.0f; row[PAINT_DAB_OPACITY] = 1.0f;
    row[PAINT_DAB_FLOW] = 1.0f; row[PAINT_DAB_ERASER] = 1.0f; row[PAINT_DAB_TIP] = -1.0f;
    row[PAINT_DAB_PIGMENT] = -1.0f; row[PAINT_DAB_PROFILE] = (float)PAINT_PROFILE_LINEAR;
}

const uint64_t BUDGET = 128000000;
const PaintDabParams PLAIN = {BUDGET, nullptr, 0, nullptr, nullptr, 0, nullptr};
const PaintDabParams SMALL = {100, nullptr, 0, nullptr, nullptr, 0, nullptr};
const PaintDabParams TIGHT = {10, nullptr, 0, nullptr, nullptr, 0, nullptr};

}  // namespace

TEST(PaintEngine, BoundsMarkTouchedTilesAndChargeWork) {
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 70.0f, 10.0f, 8.0f);
    uint8_t touched[4] = {};
    PaintResult r = paint_dab_bounds(row, 1, PAINT_DAB_COLUMNS, 128, 128, 64, &PLAIN, touched);
    ASSERT_EQ(r.status, PAINT_OK);
    EXPECT_EQ(r.dirty.x0, 62); EXPECT_EQ(r.dirty.x1, 78); EXPECT_EQ(r.dirty.y0, 2); EXPECT_EQ(r.dirty.y1, 18);
    EXPECT_EQ(r.work, 16u * 16u);
    EXPECT_EQ(touched[0], 1); EXPECT_EQ(touched[1], 1); EXPECT_EQ(touched[2], 0); EXPECT_EQ(touched[3], 0);
    EXPECT_EQ(paint_dab_bounds(row, 1, PAINT_DAB_COLUMNS, 128, 128, 64, &SMALL, nullptr).status, PAINT_ERROR_BUDGET);
    EXPECT_EQ(paint_dab_bounds(row, 1, 33, 128, 128, 64, &PLAIN, nullptr).status, PAINT_ERROR_ARGUMENT);
}

TEST(PaintEngine, HardDabCoversItsDisc) {
    TestLayer cov(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 10.0f);
    ASSERT_EQ(paint_dabs(&cov.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    double total = 0;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) total += cov.coverage(x, y);
    EXPECT_NEAR(total, M_PI * 100.0, 0.03 * M_PI * 100.0);
    EXPECT_FLOAT_EQ(cov.coverage(32, 32), 1.0f);
    EXPECT_FLOAT_EQ(cov.coverage(32, 43), 0.0f);
    // the disc is symmetric about its centre
    for (int d = 0; d < 12; d++) {
        EXPECT_FLOAT_EQ(cov.coverage(32 + d, 32), cov.coverage(31 - d, 32));
        EXPECT_FLOAT_EQ(cov.coverage(32, 32 + d), cov.coverage(32, 31 - d));
    }
}

TEST(PaintEngine, AntiAliasingStaysWithinOnePixelOfTheRim) {
    TestLayer cov(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 10.0f);
    row[PAINT_DAB_AA] = 1.0f;
    ASSERT_EQ(paint_dabs(&cov.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        double d = hypot(x + 0.5 - 32.0, y + 0.5 - 32.0);
        float c = cov.coverage(x, y);
        if (d <= 9.4) EXPECT_FLOAT_EQ(c, 1.0f) << x << "," << y;
        if (d >= 10.6) EXPECT_FLOAT_EQ(c, 0.0f) << x << "," << y;
        if (d > 9.6 && d < 10.4) { EXPECT_GT(c, 0.0f); EXPECT_LT(c, 1.0f); }
    }
}

TEST(PaintEngine, ProfilesFollowTheirCurves) {
    float row[PAINT_DAB_COLUMNS];
    // centred so that pixel (31 + d, 32) has its centre exactly d pixels away
    dab_row(row, 31.5f, 32.5f, 16.0f);
    row[PAINT_DAB_HARDNESS] = 0.5f;
    TestLayer linear(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    ASSERT_EQ(paint_dabs(&linear.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    // flat core to half the radius, then a linear fall to the rim: distance 12 of 16 gives 0.5
    EXPECT_FLOAT_EQ(linear.coverage(31 + 3, 32), 1.0f);
    EXPECT_NEAR(linear.coverage(31 + 12, 32), 0.5f, 1e-5f);
    row[PAINT_DAB_PROFILE] = (float)PAINT_PROFILE_MYPAINT;
    TestLayer mypaint(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    ASSERT_EQ(paint_dabs(&mypaint.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    // distance 8 of 16 is squared distance 0.25: first segment 1 - 0.25 * (1/0.5 - 1) = 0.75
    EXPECT_NEAR(mypaint.coverage(31 + 8, 32), 0.75f, 1e-5f);
    row[PAINT_DAB_HARDNESS] = 0.0f;
    TestLayer none(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    ASSERT_EQ(paint_dabs(&none.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    for (int x = 0; x < 64; x++) EXPECT_FLOAT_EQ(none.coverage(x, 32), 0.0f);
}

TEST(PaintEngine, EllipseFollowsAspectAndAngle) {
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 12.0f);
    row[PAINT_DAB_ASPECT] = 3.0f;
    TestLayer flat(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    ASSERT_EQ(paint_dabs(&flat.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    EXPECT_FLOAT_EQ(flat.coverage(42, 32), 1.0f);   // along the major axis
    EXPECT_FLOAT_EQ(flat.coverage(32, 37), 0.0f);   // beyond radius / aspect on the minor axis
    row[PAINT_DAB_ANGLE] = (float)(M_PI / 2.0);
    TestLayer turned(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    ASSERT_EQ(paint_dabs(&turned.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    EXPECT_FLOAT_EQ(turned.coverage(32, 42), 1.0f);
    EXPECT_FLOAT_EQ(turned.coverage(37, 32), 0.0f);
}

TEST(PaintEngine, BuildUpPaintsErasesAndLocksAlpha) {
    TestLayer layer(128, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    float rows[3 * PAINT_DAB_COLUMNS];
    dab_row(rows, 62.0f, 30.0f, 6.0f);                       // crosses the tile border
    rows[PAINT_DAB_R] = 1.0f; rows[PAINT_DAB_G] = 0.5f;
    ASSERT_EQ(paint_dabs(&layer.tiles, rows, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    float* p = layer.rgba(64, 30);
    EXPECT_FLOAT_EQ(p[0], 1.0f); EXPECT_FLOAT_EQ(p[1], 0.5f); EXPECT_FLOAT_EQ(p[3], 1.0f);
    // lock alpha recolours without adding coverage where there was none
    dab_row(rows + PAINT_DAB_COLUMNS, 62.0f, 30.0f, 12.0f);
    rows[PAINT_DAB_COLUMNS + PAINT_DAB_B] = 1.0f; rows[PAINT_DAB_COLUMNS + PAINT_DAB_LOCK_ALPHA] = 1.0f;
    ASSERT_EQ(paint_dabs(&layer.tiles, rows + PAINT_DAB_COLUMNS, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    EXPECT_FLOAT_EQ(p[2], 1.0f); EXPECT_FLOAT_EQ(p[0], 0.0f); EXPECT_FLOAT_EQ(p[3], 1.0f);
    EXPECT_FLOAT_EQ(layer.rgba(52, 30)[3], 0.0f);
    // a full-opacity eraser restores transparency
    dab_row(rows + 2 * PAINT_DAB_COLUMNS, 62.0f, 30.0f, 6.0f);
    rows[2 * PAINT_DAB_COLUMNS + PAINT_DAB_ERASER] = 0.0f;
    ASSERT_EQ(paint_dabs(&layer.tiles, rows + 2 * PAINT_DAB_COLUMNS, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    for (int c = 0; c < 4; c++) EXPECT_FLOAT_EQ(p[c], 0.0f);
}

TEST(PaintEngine, WashEqualsOneCompositeOfAccumulatedCoverage) {
    TestLayer source(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        float* s = source.rgba(x, y); s[0] = 0.1f; s[1] = 0.2f; s[2] = 0.3f; s[3] = 0.5f;
    }
    float rows[2 * PAINT_DAB_COLUMNS];
    dab_row(rows, 30.0f, 30.0f, 8.0f); dab_row(rows + PAINT_DAB_COLUMNS, 34.0f, 30.0f, 8.0f);
    rows[PAINT_DAB_FLOW] = rows[PAINT_DAB_COLUMNS + PAINT_DAB_FLOW] = 0.4f;
    TestLayer cov(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    PaintResult r = paint_dabs(&cov.tiles, rows, 2, PAINT_DAB_COLUMNS, &PLAIN);
    ASSERT_EQ(r.status, PAINT_OK);
    EXPECT_NEAR(cov.coverage(32, 30), 1.0f - 0.6f * 0.6f, 1e-6f);   // overlap accumulates, never above 1
    TestLayer out(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    PaintWashParams wash = {{1.0f, 0.0f, 0.0f}, 1.0f, 0.0f, 0.5f};
    ASSERT_EQ(paint_wash_composite(&out.tiles, &source.tiles, &cov.tiles, r.dirty, &wash).status, PAINT_OK);
    float a = cov.coverage(32, 30) * 0.5f;
    float* d = out.rgba(32, 30);
    EXPECT_NEAR(d[0], a * 1.0f + (1 - a) * 0.1f, 1e-6f);
    EXPECT_NEAR(d[3], a + (1 - a) * 0.5f, 1e-6f);
    // outside the coverage the source shows through unchanged
    EXPECT_FLOAT_EQ(out.rgba(r.dirty.x0, r.dirty.y0)[3], 0.5f);
}

TEST(PaintEngine, U8CoverageQuantizesEachDab) {
    TestLayer cov(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_U8);
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 6.0f);
    row[PAINT_DAB_FLOW] = 0.35f;
    ASSERT_EQ(paint_dabs(&cov.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    EXPECT_FLOAT_EQ(cov.coverage(32, 32), lroundf(0.35f * 255.0f) / 255.0f);
    ASSERT_EQ(paint_dabs(&cov.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    float once = lroundf(0.35f * 255.0f) / 255.0f;
    EXPECT_FLOAT_EQ(cov.coverage(32, 32), lroundf((1.0f - (1.0f - once) * 0.65f) * 255.0f) / 255.0f);
}

TEST(PaintEngine, ImportPresentRoundTripsOpaqueSrgb) {
    TestLayer layer(256, 1, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    uint8_t pixels[256 * 4], back[256 * 4];
    for (int i = 0; i < 256; i++) { pixels[4 * i] = (uint8_t)i; pixels[4 * i + 1] = (uint8_t)(255 - i); pixels[4 * i + 2] = 7; pixels[4 * i + 3] = 255; }
    PaintView in = {pixels, PAINT_ELEM_U8, 256, 1, 4, 256 * 4, 4};
    ASSERT_EQ(paint_import(&layer.tiles, &in, 0, 0, true).status, PAINT_OK);
    PaintView out = {back, PAINT_ELEM_U8, 256, 1, 4, 256 * 4, 4};
    PaintPresentParams params = {true, false, {0, 0, 0, 0}};
    PaintRect all = {0, 0, 256, 1};
    ASSERT_EQ(paint_present(&layer.tiles, all, &out, &params).status, PAINT_OK);
    EXPECT_EQ(memcmp(pixels, back, sizeof(pixels)), 0);
    // linear light is what the tiles hold
    EXPECT_NEAR(layer.rgba(128, 0)[0], 0.2158605f, 1e-5f);
}

TEST(PaintEngine, PresentCompositesOverABackground) {
    TestLayer layer(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32, false);
    uint8_t px[4];
    PaintView out = {px, PAINT_ELEM_U8, 1, 1, 4, 4, 4};
    PaintPresentParams params = {true, true, {1.0f, 1.0f, 1.0f, 1.0f}};
    PaintRect one = {5, 5, 6, 6};
    ASSERT_EQ(paint_present(&layer.tiles, one, &out, &params).status, PAINT_OK);   // empty tile shows the paper
    EXPECT_EQ(px[0], 255); EXPECT_EQ(px[3], 255);
    PaintView wrong = {px, PAINT_ELEM_U8, 2, 1, 4, 8, 4};
    EXPECT_EQ(paint_present(&layer.tiles, one, &wrong, &params).status, PAINT_ERROR_ARGUMENT);
}

TEST(PaintEngine, RefusesWithoutWritingAnything) {
    TestLayer layer(128, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    free(layer.views[1].data); layer.views[1].data = nullptr;    // the right tile is not supplied
    float rows[2 * PAINT_DAB_COLUMNS];
    dab_row(rows, 20.0f, 20.0f, 4.0f); dab_row(rows + PAINT_DAB_COLUMNS, 70.0f, 20.0f, 4.0f);
    EXPECT_EQ(paint_dabs(&layer.tiles, rows, 2, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_ERROR_ARGUMENT);
    EXPECT_FLOAT_EQ(layer.rgba(20, 20)[3], 0.0f);                 // the first dab was not painted either
    dab_row(rows, 20.0f, 20.0f, 4.0f);
    rows[PAINT_DAB_SMUDGE] = 0.5f;
    PaintResult r = paint_dabs(&layer.tiles, rows, 1, PAINT_DAB_COLUMNS, &PLAIN);
    EXPECT_EQ(r.status, PAINT_ERROR_ARGUMENT);
    EXPECT_NE(r.reason, nullptr);
    dab_row(rows, 20.0f, 20.0f, 4.0f);
    rows[PAINT_DAB_X] = NAN;
    EXPECT_EQ(paint_dabs(&layer.tiles, rows, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_ERROR_ARGUMENT);
    dab_row(rows, 20.0f, 20.0f, 4.0f);
    EXPECT_EQ(paint_dabs(&layer.tiles, rows, 1, PAINT_DAB_COLUMNS, &TIGHT).status, PAINT_ERROR_BUDGET);
    EXPECT_FLOAT_EQ(layer.rgba(20, 20)[3], 0.0f);
    layer.views[0].channels = 3;                                  // malformed tile view
    dab_row(rows, 20.0f, 20.0f, 4.0f);
    EXPECT_EQ(paint_dabs(&layer.tiles, rows, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_ERROR_ARGUMENT);
}

namespace {

// an opaque layer filled with one premultiplied colour.
void fill(TestLayer& layer, float r, float g, float b, float a) {
    for (int y = 0; y < layer.tiles.height; y++) for (int x = 0; x < layer.tiles.width; x++) {
        float* p = layer.rgba(x, y); p[0] = r * a; p[1] = g * a; p[2] = b * a; p[3] = a;
    }
}

}  // namespace

TEST(PaintEngine, SmudgePicksUpTheCanvasAndKeepsBuckets) {
    TestLayer layer(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    fill(layer, 1.0f, 0.0f, 0.0f, 1.0f);
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 6.0f);
    row[PAINT_DAB_B] = 1.0f;                                     // blue brush
    row[PAINT_DAB_SMUDGE] = 1.0f; row[PAINT_DAB_SMUDGE_LENGTH] = 0.0f; row[PAINT_DAB_SMUDGE_RADIUS] = 1.0f;
    float buckets[2 * PAINT_SMUDGE_FIELDS] = {}, resolved[4];
    PaintDabParams params = PLAIN;
    params.smudge = buckets; params.smudge_buckets = 2; params.resolved = resolved;
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    // full smudge with an immediate pickup carries the red canvas, not the blue brush; like
    // libmypaint, a pickup keeps at least 1% of the old (here transparent) bucket
    EXPECT_NEAR(resolved[0], 1.0f, 1e-5f); EXPECT_NEAR(resolved[2], 0.0f, 1e-5f); EXPECT_NEAR(resolved[3], 0.99f, 1e-5f);
    EXPECT_NEAR(layer.rgba(32, 32)[0], 0.99f, 1e-5f);  // a full dab moves the pixel to 0.99 red
    EXPECT_NEAR(buckets[3], 0.99f, 1e-5f);                      // bucket 0 holds nearly opaque red
    EXPECT_FLOAT_EQ(buckets[PAINT_SMUDGE_FIELDS + 3], 0.0f);     // bucket 1 untouched
    // a bucket that picked up transparency makes the dab erase toward it (libmypaint)
    TestLayer empty(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    float fresh[PAINT_SMUDGE_FIELDS] = {};
    params.smudge = fresh; params.smudge_buckets = 1;
    ASSERT_EQ(paint_dabs(&empty.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_FLOAT_EQ(resolved[3], 0.0f);
    EXPECT_FLOAT_EQ(empty.rgba(32, 32)[3], 0.0f);
    // without smudge the brush colour lands and the target alpha is the eraser column
    row[PAINT_DAB_SMUDGE] = 0.0f; row[PAINT_DAB_SMUDGE_RADIUS] = 0.0f; row[PAINT_DAB_SMUDGE_LENGTH] = 1.0f;
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_NEAR(layer.rgba(32, 32)[2], 1.0f, 1e-5f);
    EXPECT_FLOAT_EQ(resolved[3], 1.0f);
}

TEST(PaintEngine, SmudgeLengthBlendsOldAndNewPickups) {
    TestLayer layer(128, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 128; x++) {
        float* p = layer.rgba(x, y); p[3] = 1.0f; p[x < 64 ? 0 : 2] = 1.0f;   // red left, blue right
    }
    float rows[2 * PAINT_DAB_COLUMNS], buckets[PAINT_SMUDGE_FIELDS] = {}, resolved[8];
    for (int i = 0; i < 2; i++) {
        float* row = rows + i * PAINT_DAB_COLUMNS;
        dab_row(row, i == 0 ? 20.0f : 100.0f, 32.0f, 4.0f);
        row[PAINT_DAB_SMUDGE] = 1.0f; row[PAINT_DAB_SMUDGE_LENGTH] = 0.5f; row[PAINT_DAB_SMUDGE_RADIUS] = 1.0f;
    }
    PaintDabParams params = PLAIN;
    params.smudge = buckets; params.smudge_buckets = 1; params.resolved = resolved;
    ASSERT_EQ(paint_dabs(&layer.tiles, rows, 2, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    // the second dab mixes half the carried red with half the blue it picks up
    EXPECT_NEAR(resolved[4], 0.5f * 0.5f / 0.75f, 1e-4f);
    EXPECT_NEAR(resolved[6], 0.5f / 0.75f, 1e-4f);
}

TEST(PaintEngine, BitmapTipsAndPaperGrainShapeTheMask) {
    float texels[4 * 4] = {};
    for (int y = 0; y < 2; y++) for (int x = 0; x < 2; x++) texels[y * 4 + x] = 1.0f;   // top-left quadrant
    PaintView tip = {texels, PAINT_ELEM_F32, 4, 4, 1, 4, 1};
    TestLayer cov(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 16.0f);
    row[PAINT_DAB_TIP] = 0.0f;
    PaintDabParams params = PLAIN;
    params.tips = &tip; params.tip_count = 1;
    ASSERT_EQ(paint_dabs(&cov.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_FLOAT_EQ(cov.coverage(22, 22), 1.0f);
    EXPECT_FLOAT_EQ(cov.coverage(42, 42), 0.0f);
    row[PAINT_DAB_TIP] = 1.0f;
    EXPECT_EQ(paint_dabs(&cov.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_ERROR_ARGUMENT);
    // grain: peaks receive the dab fully, valleys by 1 - grain
    float heights[2] = {0.0f, 1.0f};
    PaintView paper = {heights, PAINT_ELEM_F32, 2, 1, 1, 2, 1};
    TestLayer grained(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    dab_row(row, 32.0f, 32.0f, 8.0f);
    row[PAINT_DAB_GRAIN] = 1.0f;
    EXPECT_EQ(paint_dabs(&grained.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_ERROR_ARGUMENT);
    params = PLAIN; params.paper = &paper;
    ASSERT_EQ(paint_dabs(&grained.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_FLOAT_EQ(grained.coverage(32, 32), 0.0f);
    EXPECT_FLOAT_EQ(grained.coverage(33, 32), 1.0f);
}

TEST(PaintEngine, BlendModesColorizeAndPosterize) {
    float row[PAINT_DAB_COLUMNS];
    dab_row(row, 32.0f, 32.0f, 6.0f);
    row[PAINT_DAB_R] = 0.5f; row[PAINT_DAB_G] = 1.0f; row[PAINT_DAB_B] = 0.0f;
    row[PAINT_DAB_BLEND] = (float)PAINT_BLEND_MULTIPLY;
    TestLayer multiply(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    fill(multiply, 0.5f, 0.5f, 0.5f, 1.0f);
    ASSERT_EQ(paint_dabs(&multiply.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    float* p = multiply.rgba(32, 32);
    EXPECT_NEAR(p[0], 0.25f, 1e-5f); EXPECT_NEAR(p[1], 0.5f, 1e-5f); EXPECT_NEAR(p[2], 0.0f, 1e-5f);
    row[PAINT_DAB_BLEND] = (float)PAINT_BLEND_COUNT;
    EXPECT_EQ(paint_dabs(&multiply.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_ERROR_ARGUMENT);
    // colorize takes the brush hue and saturation and keeps luminosity and coverage
    dab_row(row, 32.0f, 32.0f, 6.0f);
    row[PAINT_DAB_R] = 1.0f; row[PAINT_DAB_COLORIZE] = 1.0f;
    TestLayer tinted(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    fill(tinted, 0.4f, 0.4f, 0.4f, 0.5f);
    ASSERT_EQ(paint_dabs(&tinted.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    p = tinted.rgba(32, 32);
    EXPECT_FLOAT_EQ(p[3], 0.5f);
    EXPECT_NEAR(0.3f * p[0] / p[3] + 0.59f * p[1] / p[3] + 0.11f * p[2] / p[3], 0.4f, 1e-4f);
    EXPECT_GT(p[0], p[1]);
    // posterize to two levels moves 0.3 grey to 0.5
    dab_row(row, 32.0f, 32.0f, 6.0f);
    row[PAINT_DAB_POSTERIZE] = 1.0f; row[PAINT_DAB_POSTERIZE_LEVELS] = 2.0f;
    TestLayer poster(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    fill(poster, 0.3f, 0.3f, 0.3f, 1.0f);
    ASSERT_EQ(paint_dabs(&poster.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_OK);
    EXPECT_NEAR(poster.rgba(32, 32)[0], 0.5f, 1e-5f);
}

TEST(PaintEngine, ColourShiftsApplyAfterSmudge) {
    float row[PAINT_DAB_COLUMNS], resolved[4];
    dab_row(row, 32.0f, 32.0f, 4.0f);
    row[PAINT_DAB_R] = 1.0f; row[PAINT_DAB_DH] = 1.0f / 3.0f;      // red rotated a third of the circle
    PaintDabParams params = PLAIN;
    params.resolved = resolved;
    TestLayer layer(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_NEAR(resolved[0], 0.0f, 1e-4f); EXPECT_NEAR(resolved[1], 1.0f, 1e-4f); EXPECT_NEAR(resolved[2], 0.0f, 1e-4f);
    dab_row(row, 32.0f, 32.0f, 4.0f);
    row[PAINT_DAB_R] = row[PAINT_DAB_G] = row[PAINT_DAB_B] = 1.0f; row[PAINT_DAB_DV] = -0.5f;
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_NEAR(resolved[0], 0.5f, 1e-4f); EXPECT_NEAR(resolved[2], 0.5f, 1e-4f);
}

TEST(PaintEngine, SampleAveragesUnderAFootprint) {
    TestLayer layer(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) {
        float* p = layer.rgba(x, y);
        if (x < 32) { p[0] = 1.0f; p[3] = 1.0f; }                 // opaque red left, empty right
    }
    float out[4];
    ASSERT_EQ(paint_sample(&layer.tiles, 32.0f, 32.0f, 8.0f, out).status, PAINT_OK);
    EXPECT_NEAR(out[3], 0.5f, 0.02f);                              // half the footprint is covered
    EXPECT_NEAR(out[0], 1.0f, 1e-5f);                              // straight colour of what is there
    ASSERT_EQ(paint_sample(&layer.tiles, 60.0f, 10.0f, 2.0f, out).status, PAINT_OK);
    EXPECT_FLOAT_EQ(out[3], 0.0f);
    EXPECT_EQ(paint_sample(&layer.tiles, NAN, 10.0f, 2.0f, out).status, PAINT_ERROR_ARGUMENT);
}

// ---- pigment medium (PE8) ----

namespace {

// a 3-band pigment layer whose allocated tiles start clean over a white substrate.
struct PigmentLayer : TestLayer {
    PigmentLayer(int32_t width, int32_t height, float substrate = 1.0f, bool allocate = true)
        : TestLayer(width, height, 64, PAINT_FORMAT_PIGMENT, PAINT_ELEM_F32, allocate, 3) {
        for (int b = 0; b < 3; b++) tiles.substrate[b] = substrate;
        for (int32_t i = 0; allocate && i < tiles.tiles_x * tiles.tiles_y; i++)
            for (int32_t k = 0; k < 64 * 64; k++)
                for (int b = 0; b < 3; b++) ((float*)views[i].data)[k * 11 + paint_pigment_r(3) + b] = substrate;
    }
};

// Washes' colour-to-KS inversion (procart.watercolor.pigment): K/S = (1 - R)^2 / 2R at a scattering of 3.
void km_pigment(const float rgb[3], float* row) {
    for (int b = 0; b < 3; b++) {
        float r = rgb[b] < 0.02f ? 0.02f : (rgb[b] > 0.992f ? 0.992f : rgb[b]);
        row[3 + b] = 3.0f;
        row[b] = (1.0f - r) * (1.0f - r) / (2.0f * r) * 3.0f;
    }
}

void pigment_row(float* row, float x, float y, float radius, int pigment, float amount) {
    dab_row(row, x, y, radius);
    row[PAINT_DAB_PIGMENT] = (float)pigment; row[PAINT_DAB_AMOUNT] = amount; row[PAINT_DAB_THICKNESS] = amount;
}

PaintDabParams pigment_params(const float* table, int32_t count) {
    PaintDabParams params = PLAIN;
    params.pigment_bands = 3; params.pigments = table; params.pigment_count = count;
    return params;
}

void present_pixel(const PaintTiles* layer, int32_t x, int32_t y, const PaintPresentParams* params, uint8_t out[4]) {
    PaintView view = {out, PAINT_ELEM_U8, 1, 1, 4, 4, 4};
    PaintRect rect = {x, y, x + 1, y + 1};
    ASSERT_EQ(paint_present(layer, rect, &view, params).status, PAINT_OK);
}

const PaintPresentParams LINEAR_OUT = {false, false, {0, 0, 0, 0}, false, {0, 0, 1}, 0, 0, 0, 1, nullptr};

}  // namespace

TEST(PaintEngine, PigmentCleanLayerShowsItsSubstrate) {
    PigmentLayer layer(64, 64, 0.5f);
    PigmentLayer sparse(128, 64, 0.25f, false);   // clean (empty) tiles read as the layer's substrate
    uint8_t px[4];
    present_pixel(&layer.tiles, 10, 10, &LINEAR_OUT, px);
    EXPECT_EQ(px[0], 128); EXPECT_EQ(px[1], 128); EXPECT_EQ(px[2], 128); EXPECT_EQ(px[3], 255);
    present_pixel(&sparse.tiles, 100, 10, &LINEAR_OUT, px);
    EXPECT_EQ(px[0], 64); EXPECT_EQ(px[3], 255);
    // a film with thickness but no pigment (a clear medium) shows the substrate too
    layer.pigment(10, 10)[paint_pigment_t(3)] = 2.0f;
    present_pixel(&layer.tiles, 10, 10, &LINEAR_OUT, px);
    EXPECT_EQ(px[0], 128);
}

TEST(PaintEngine, PigmentKubelkaMunkLimits) {
    PigmentLayer layer(64, 64, 0.5f);
    uint8_t px[4];
    float* p = layer.pigment(1, 1);
    p[3] = p[4] = p[5] = 400.0f;                                   // a thick pure scatterer is white
    present_pixel(&layer.tiles, 1, 1, &LINEAR_OUT, px);
    EXPECT_GE(px[0], 254);
    p = layer.pigment(2, 1);
    p[0] = p[1] = p[2] = 400.0f; p[3] = p[4] = p[5] = 0.01f;       // a thick strong absorber is black
    present_pixel(&layer.tiles, 2, 1, &LINEAR_OUT, px);
    EXPECT_LE(px[0], 1);
    p = layer.pigment(3, 1);
    p[0] = 0.5f;                                                   // a pure absorber passes light twice
    present_pixel(&layer.tiles, 3, 1, &LINEAR_OUT, px);
    EXPECT_NEAR(px[0], lround(0.5 * exp(-1.0) * 255.0), 1);
    EXPECT_EQ(px[1], 128);
    // an opaque film of one pigment shows R_inf = 1 + K/S - sqrt((K/S)^2 + 2 K/S)
    p = layer.pigment(4, 1);
    p[0] = 60.0f; p[3] = 120.0f;                                   // K/S = 0.5, S t = 120
    present_pixel(&layer.tiles, 4, 1, &LINEAR_OUT, px);
    EXPECT_NEAR(px[0], lround((1.5 - sqrt(1.25)) * 255.0), 1);
}

TEST(PaintEngine, PigmentDepositIsAdditiveAndCommutes) {
    float yellow[3] = {0.9f, 0.75f, 0.05f}, blue[3] = {0.05f, 0.15f, 0.8f}, table[12];
    km_pigment(yellow, table); km_pigment(blue, table + 6);
    PaintDabParams params = pigment_params(table, 2);
    float rows[2 * PAINT_DAB_COLUMNS];
    pigment_row(rows, 32.0f, 32.0f, 10.0f, 0, 0.6f);
    pigment_row(rows + PAINT_DAB_COLUMNS, 32.0f, 32.0f, 10.0f, 1, 0.6f);
    PigmentLayer forward(64, 64), backward(64, 64);
    ASSERT_EQ(paint_dabs(&forward.tiles, rows, 2, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    ASSERT_EQ(paint_dabs(&backward.tiles, rows + PAINT_DAB_COLUMNS, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    ASSERT_EQ(paint_dabs(&backward.tiles, rows, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    const float* f = forward.pigment(32, 32);
    for (int k = 0; k < 11; k++) EXPECT_EQ(f[k], backward.pigment(32, 32)[k]) << k;
    // the film holds both deposits: K and S add, and so does the thickness
    EXPECT_FLOAT_EQ(f[0], 0.6f * (table[0] + table[6]));
    EXPECT_FLOAT_EQ(f[paint_pigment_t(3)], 1.2f);
    EXPECT_FLOAT_EQ(f[paint_pigment_r(3)], 1.0f);                  // the substrate is untouched
}

TEST(PaintEngine, PigmentBlueOverYellowMixesTowardGreen) {
    float yellow[3] = {0.9f, 0.75f, 0.05f}, blue[3] = {0.05f, 0.15f, 0.8f}, table[12];
    km_pigment(yellow, table); km_pigment(blue, table + 6);
    PaintDabParams params = pigment_params(table, 2);
    PigmentLayer layer(64, 64);
    float row[PAINT_DAB_COLUMNS];
    pigment_row(row, 20.0f, 32.0f, 14.0f, 0, 1.0f);               // a yellow patch
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    pigment_row(row, 44.0f, 32.0f, 14.0f, 1, 1.0f);               // blue overlapping it
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    uint8_t y[4], b[4], g[4];
    present_pixel(&layer.tiles, 10, 32, &LINEAR_OUT, y);
    present_pixel(&layer.tiles, 54, 32, &LINEAR_OUT, b);
    present_pixel(&layer.tiles, 32, 32, &LINEAR_OUT, g);
    EXPECT_GT(y[0], y[2]); EXPECT_GT(b[2], b[0]);                 // the unmixed patches keep their hues
    EXPECT_GT(g[1], g[0]); EXPECT_GT(g[1], g[2]);                 // the overlap is green
    EXPECT_GT(g[1] - g[0], 20); EXPECT_GT(g[1] - g[2], 20);
}

TEST(PaintEngine, PigmentDryingPreservesAppearance) {
    float red[3] = {0.8f, 0.1f, 0.1f}, table[6];
    km_pigment(red, table);
    PaintDabParams params = pigment_params(table, 1);
    PigmentLayer whole(64, 64), halves(64, 64);
    float row[PAINT_DAB_COLUMNS];
    pigment_row(row, 32.0f, 32.0f, 12.0f, 0, 0.3f);               // a thin glaze
    ASSERT_EQ(paint_dabs(&whole.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    ASSERT_EQ(paint_dabs(&halves.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    uint8_t before[4], after[4], split[4];
    present_pixel(&whole.tiles, 32, 32, &LINEAR_OUT, before);
    PaintRect all = {0, 0, 64, 64};
    ASSERT_EQ(paint_pigment_dry(&whole.tiles, all, 1.0f).status, PAINT_OK);
    ASSERT_EQ(paint_pigment_dry(&halves.tiles, all, 0.5f).status, PAINT_OK);
    ASSERT_EQ(paint_pigment_dry(&halves.tiles, all, 1.0f).status, PAINT_OK);
    present_pixel(&whole.tiles, 32, 32, &LINEAR_OUT, after);
    present_pixel(&halves.tiles, 32, 32, &LINEAR_OUT, split);
    for (int c = 0; c < 3; c++) { EXPECT_NEAR(after[c], before[c], 1) << c; EXPECT_NEAR(split[c], before[c], 1) << c; }
    const float* p = whole.pigment(32, 32);
    EXPECT_FLOAT_EQ(p[0], 0.0f); EXPECT_FLOAT_EQ(p[paint_pigment_t(3)], 0.0f);   // no wet film is left
    EXPECT_NEAR(p[paint_pigment_h(3)], 0.3f, 1e-6f);                            // its body became relief
    // a glaze over the dried layer stays transparent: the red substrate shows through blue
    float blue[3] = {0.1f, 0.1f, 0.8f}, glaze[6];
    km_pigment(blue, glaze);
    PaintDabParams glazing = pigment_params(glaze, 1);
    pigment_row(row, 32.0f, 32.0f, 12.0f, 0, 0.1f);
    ASSERT_EQ(paint_dabs(&whole.tiles, row, 1, PAINT_DAB_COLUMNS, &glazing).status, PAINT_OK);
    uint8_t glazed[4];
    present_pixel(&whole.tiles, 32, 32, &LINEAR_OUT, glazed);
    // an opaque film of the glaze's blue shows R_inf(K/S = 4.05) = 0.0995 in red, about 25
    EXPECT_LT(glazed[0], before[0]); EXPECT_GT(glazed[0], 31);
    EXPECT_EQ(paint_pigment_dry(&whole.tiles, all, 0.0f).status, PAINT_ERROR_ARGUMENT);
}

TEST(PaintEngine, PigmentLightingKeepsFlatFilmAndShadesSlopes) {
    PigmentLayer layer(64, 64, 0.5f);
    // a ridge of relief along x = 32
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++)
        layer.pigment(x, y)[paint_pigment_h(3)] = fmaxf(0.0f, 4.0f - fabsf((float)x - 32.0f));
    PaintPresentParams lit = LINEAR_OUT;
    lit.has_lighting = true; lit.light[0] = -1.0f; lit.light[1] = 0.0f; lit.light[2] = 1.0f;   // from the left
    lit.relief = 1.0f; lit.diffuse = 0.8f; lit.specular = 0.3f; lit.shininess = 16.0f;
    uint8_t flat[4], plain[4], left[4], right[4];
    present_pixel(&layer.tiles, 5, 10, &lit, flat);
    present_pixel(&layer.tiles, 5, 10, &LINEAR_OUT, plain);
    EXPECT_EQ(flat[0], plain[0]);                                 // a flat region is unchanged
    present_pixel(&layer.tiles, 30, 10, &lit, left);              // the slope facing the light
    present_pixel(&layer.tiles, 34, 10, &lit, right);             // the slope facing away
    EXPECT_GT(left[0], plain[0]); EXPECT_LT(right[0], plain[0]);
    PaintPresentParams bad = lit;
    bad.light[2] = 0.0f;
    uint8_t px[4];
    PaintView view = {px, PAINT_ELEM_U8, 1, 1, 4, 4, 4};
    PaintRect one = {0, 0, 1, 1};
    EXPECT_EQ(paint_present(&layer.tiles, one, &view, &bad).status, PAINT_ERROR_ARGUMENT);
    TestLayer rgba(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    EXPECT_EQ(paint_present(&rgba.tiles, one, &view, &lit).status, PAINT_ERROR_ARGUMENT);
}

TEST(PaintEngine, PigmentSmudgeCarriesTheFilm) {
    float yellow[3] = {0.9f, 0.75f, 0.05f}, table[6];
    km_pigment(yellow, table);
    PaintDabParams params = pigment_params(table, 1);
    float buckets[7] = {0};
    params.smudge = buckets; params.smudge_buckets = 1;
    PigmentLayer layer(64, 64);
    float row[PAINT_DAB_COLUMNS];
    pigment_row(row, 16.0f, 32.0f, 10.0f, 0, 1.0f);
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    float before[11];
    memcpy(before, layer.pigment(16, 32), sizeof(before));
    // a pure smudge dab (amount 0) on the patch picks it up and leaves a uniform film as it was,
    // up to the 1% of its old contents that a bucket always keeps (smudge_length floor, as in rgba)
    pigment_row(row, 16.0f, 32.0f, 3.0f, 0, 0.0f);
    row[PAINT_DAB_SMUDGE] = 1.0f; row[PAINT_DAB_SMUDGE_LENGTH] = 0.0f; row[PAINT_DAB_SMUDGE_RADIUS] = 1.0f;
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    for (int k = 0; k < 7; k++) EXPECT_NEAR(layer.pigment(16, 32)[k], before[k], 0.011f * before[k] + 1e-6f) << k;
    EXPECT_GT(buckets[0], 0.0f);
    // dragged onto clean paper with smudge_length 1 (keep the bucket), it lays the carried film down
    row[PAINT_DAB_X] = 48.0f; row[PAINT_DAB_SMUDGE_LENGTH] = 1.0f;
    ASSERT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_OK);
    EXPECT_NEAR(layer.pigment(48, 32)[0], buckets[0], 1e-4f * buckets[0]);
    EXPECT_GT(layer.pigment(48, 32)[paint_pigment_t(3)], 0.5f);
}

TEST(PaintEngine, PigmentRefusesRgbaModesAndMismatches) {
    float table[6] = {1, 1, 1, 1, 1, 1};
    PaintDabParams params = pigment_params(table, 1);
    PigmentLayer layer(64, 64);
    TestLayer rgba(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    float row[PAINT_DAB_COLUMNS];
    pigment_row(row, 32.0f, 32.0f, 4.0f, 1, 1.0f);                // no such pigment
    EXPECT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_ERROR_ARGUMENT);
    pigment_row(row, 32.0f, 32.0f, 4.0f, 0, 1.0f);
    row[PAINT_DAB_LOCK_ALPHA] = 0.5f;
    EXPECT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_ERROR_ARGUMENT);
    pigment_row(row, 32.0f, 32.0f, 4.0f, 0, 1.0f);
    EXPECT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &PLAIN).status, PAINT_ERROR_ARGUMENT);   // no table
    EXPECT_EQ(paint_dabs(&rgba.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_ERROR_ARGUMENT);   // not a pigment layer
    params.pigment_bands = 2;
    EXPECT_EQ(paint_dabs(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, &params).status, PAINT_ERROR_ARGUMENT);  // band mismatch
    // nothing was written by the refused calls
    for (int k = 0; k < 7; k++) EXPECT_FLOAT_EQ(layer.pigment(32, 32)[k], 0.0f);
    uint8_t px[4];
    PaintView view = {px, PAINT_ELEM_U8, 1, 1, 4, 4, 4};
    PaintRect one = {0, 0, 1, 1};
    PaintPresentParams over = LINEAR_OUT;
    over.has_background = true; over.background[3] = 1.0f;
    EXPECT_EQ(paint_present(&layer.tiles, one, &view, &over).status, PAINT_ERROR_ARGUMENT);
}


// ---- bristle trails (PE10) ----

namespace {

// n bristles in a row across the brush, each carrying `ink` (rgba colour or pigment film).
void bristle_state(float* state, int32_t n, int32_t columns, const float* ink, int32_t ink_fields, float load) {
    for (int32_t j = 0; j < n; j++) {
        float* b = state + j * columns;
        for (int32_t k = 0; k < columns; k++) b[k] = 0.0f;
        b[PAINT_BRISTLE_OX] = 0.0f; b[PAINT_BRISTLE_OY] = n > 1 ? -0.8f + 1.6f * (float)j / (float)(n - 1) : 0.0f;
        b[PAINT_BRISTLE_STIFFNESS] = 0.2f; b[PAINT_BRISTLE_WIDTH] = 2.0f; b[PAINT_BRISTLE_LOAD] = load;
        for (int32_t k = 0; k < ink_fields; k++) b[PAINT_BRISTLE_INK + k] = ink[k];
    }
}

// a straight horizontal path of `count` brush positions.
void bristle_path(float* rows, int32_t count, float x0, float step, float y, float radius) {
    for (int32_t i = 0; i < count; i++) dab_row(rows + i * PAINT_DAB_COLUMNS, x0 + step * (float)i, y, radius);
}

const PaintBristleParams BRISTLES_RGBA = {BUDGET, nullptr, 0.01f, 0.0f, 0};

}  // namespace

TEST(PaintEngine, BristleFirstRowOnlyPlacesTheTips) {
    TestLayer layer(64, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    const int32_t columns = paint_bristle_columns(0);
    float ink[3] = {1.0f, 0.0f, 0.0f}, state[4 * 11], row[PAINT_DAB_COLUMNS];
    ASSERT_EQ(columns, 11);
    bristle_state(state, 4, columns, ink, 3, 1.0f);
    bristle_path(row, 1, 32.0f, 0.0f, 32.0f, 8.0f);
    PaintResult r = paint_bristles(&layer.tiles, row, 1, PAINT_DAB_COLUMNS, state, 4, columns, &BRISTLES_RGBA);
    ASSERT_EQ(r.status, PAINT_OK);
    EXPECT_EQ(r.work, 0u);
    for (int32_t j = 0; j < 4; j++) {
        EXPECT_EQ(state[j * columns + PAINT_BRISTLE_PLACED], 1.0f);
        EXPECT_FLOAT_EQ(state[j * columns + PAINT_BRISTLE_X], 32.0f);
    }
    EXPECT_FLOAT_EQ(state[PAINT_BRISTLE_Y], 32.0f - 0.8f * 8.0f);   // bristle 0 sits at the brush's top edge
    for (int y = 0; y < 64; y++) for (int x = 0; x < 64; x++) EXPECT_FLOAT_EQ(layer.rgba(x, y)[3], 0.0f);
}

TEST(PaintEngine, BristleLoadDepletesAlongTheStroke) {
    TestLayer layer(256, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    const int32_t columns = paint_bristle_columns(0);
    float ink[3] = {0.0f, 0.0f, 1.0f}, state[3 * 11], rows[60 * PAINT_DAB_COLUMNS];
    bristle_state(state, 3, columns, ink, 3, 0.6f);
    bristle_path(rows, 60, 10.0f, 4.0f, 32.0f, 10.0f);
    PaintBristleParams params = BRISTLES_RGBA;
    params.depletion = 0.006f;                       // load(s) = 0.6 exp(-0.006 s): a quarter is left after 240 px
    float previous[3] = {0.6f, 0.6f, 0.6f};
    for (int32_t i = 0; i < 60; i++) {
        ASSERT_EQ(paint_bristles(&layer.tiles, rows + i * PAINT_DAB_COLUMNS, 1, PAINT_DAB_COLUMNS, state, 3, columns, &params).status, PAINT_OK);
        for (int32_t j = 0; j < 3; j++) {
            float load = state[j * columns + PAINT_BRISTLE_LOAD];
            EXPECT_LE(load, previous[j]) << i << " " << j;           // the load never grows
            if (i > 1) EXPECT_LT(load, previous[j]) << i << " " << j; // and falls while the bristle draws
            previous[j] = load;
        }
    }
    // the trail of the middle bristle (y = 32) fades from the start of the stroke to its end
    float early = layer.rgba(30, 32)[3], late = layer.rgba(230, 32)[3];
    EXPECT_GT(early, 0.4f);
    EXPECT_LT(late, 0.5f * early);
}

TEST(PaintEngine, BristleSplitPathEqualsWholePath) {
    const int32_t columns = paint_bristle_columns(0);
    float ink[3] = {0.2f, 0.6f, 0.1f}, whole_state[5 * 11], split_state[5 * 11], rows[30 * PAINT_DAB_COLUMNS];
    bristle_state(whole_state, 5, columns, ink, 3, 1.0f);
    memcpy(split_state, whole_state, sizeof(whole_state));
    for (int32_t i = 0; i < 30; i++) {
        dab_row(rows + i * PAINT_DAB_COLUMNS, 20.0f + 5.0f * i, 30.0f + 10.0f * sinf(0.3f * i), 9.0f);
        rows[i * PAINT_DAB_COLUMNS + PAINT_DAB_ANGLE] = 0.1f * i;
        rows[i * PAINT_DAB_COLUMNS + PAINT_DAB_AA] = 1.0f;
    }
    PaintBristleParams params = BRISTLES_RGBA;
    params.pickup = 0.3f;
    TestLayer whole(192, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32), split(192, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    ASSERT_EQ(paint_bristles(&whole.tiles, rows, 30, PAINT_DAB_COLUMNS, whole_state, 5, columns, &params).status, PAINT_OK);
    ASSERT_EQ(paint_bristles(&split.tiles, rows, 13, PAINT_DAB_COLUMNS, split_state, 5, columns, &params).status, PAINT_OK);
    ASSERT_EQ(paint_bristles(&split.tiles, rows + 13 * PAINT_DAB_COLUMNS, 17, PAINT_DAB_COLUMNS, split_state, 5, columns, &params).status, PAINT_OK);
    for (int32_t k = 0; k < 5 * columns; k++) EXPECT_EQ(whole_state[k], split_state[k]) << k;
    for (int y = 0; y < 64; y++) for (int x = 0; x < 192; x++)
        for (int c = 0; c < 4; c++) ASSERT_EQ(whole.rgba(x, y)[c], split.rgba(x, y)[c]) << x << "," << y;
}

TEST(PaintEngine, BristlePickupMixesTheCanvasIntoItsInk) {
    TestLayer layer(128, 64, 64, PAINT_FORMAT_RGBA, PAINT_ELEM_F32);
    for (int y = 0; y < 64; y++) for (int x = 60; x < 128; x++) { float* p = layer.rgba(x, y); p[2] = 1.0f; p[3] = 1.0f; }   // blue right half
    const int32_t columns = paint_bristle_columns(0);
    float ink[3] = {1.0f, 0.0f, 0.0f}, state[11], rows[25 * PAINT_DAB_COLUMNS];
    bristle_state(state, 1, columns, ink, 3, 100.0f);
    bristle_path(rows, 25, 10.0f, 4.5f, 32.0f, 4.0f);
    PaintBristleParams params = BRISTLES_RGBA;
    params.pickup = 0.2f;
    ASSERT_EQ(paint_bristles(&layer.tiles, rows, 25, PAINT_DAB_COLUMNS, state, 1, columns, &params).status, PAINT_OK);
    EXPECT_LT(state[PAINT_BRISTLE_INK], 0.5f);       // the red ink has taken up blue
    EXPECT_GT(state[PAINT_BRISTLE_INK + 2], 0.5f);
    EXPECT_GT(layer.rgba(20, 32)[0], 0.9f);          // the trail before the blue is still red
}

TEST(PaintEngine, BristlesDepositFilmOnPigmentLayers) {
    PigmentLayer layer(128, 64);
    const int32_t columns = paint_bristle_columns(3);
    ASSERT_EQ(columns, 15);
    float film[7] = {2.0f, 0.5f, 0.1f, 1.0f, 1.0f, 1.0f, 0.5f}, state[2 * 15], rows[20 * PAINT_DAB_COLUMNS];
    bristle_state(state, 2, columns, film, 7, 1.0f);
    bristle_path(rows, 20, 10.0f, 5.0f, 32.0f, 6.0f);
    PaintBristleParams params = {BUDGET, nullptr, 0.0f, 0.0f, 3};
    ASSERT_EQ(paint_bristles(&layer.tiles, rows, 20, PAINT_DAB_COLUMNS, state, 2, columns, &params).status, PAINT_OK);
    // a pixel the trail fully covers gains exactly the ink (paint per unit coverage)
    const float* p = layer.pigment(50, (int32_t)floorf(32.0f - 0.8f * 6.0f));
    EXPECT_NEAR(p[0], 2.0f, 1e-5f); EXPECT_NEAR(p[paint_pigment_t(3)], 0.5f, 1e-5f);
    EXPECT_FLOAT_EQ(layer.pigment(50, 32)[0], 0.0f);   // between the two bristles nothing lands
    // the wrong state layout or a coverage target is refused
    EXPECT_EQ(paint_bristles(&layer.tiles, rows, 20, PAINT_DAB_COLUMNS, state, 2, 11, &params).status, PAINT_ERROR_ARGUMENT);
    TestLayer cov(64, 64, 64, PAINT_FORMAT_COVERAGE, PAINT_ELEM_F32);
    PaintBristleParams plain = BRISTLES_RGBA;
    EXPECT_EQ(paint_bristles(&cov.tiles, rows, 20, PAINT_DAB_COLUMNS, state, 2, 11, &plain).status, PAINT_ERROR_ARGUMENT);
    rows[PAINT_DAB_TIP] = 0.0f;                        // bitmap tips are for dabs
    EXPECT_EQ(paint_bristles(&layer.tiles, rows, 20, PAINT_DAB_COLUMNS, state, 2, columns, &params).status, PAINT_ERROR_ARGUMENT);
}

// ---- release-build throughput (Lambda_Pkg_Paint §5); run with --gtest_also_run_disabled_tests ----

namespace {

double seconds_now() {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (double)t.tv_sec + 1e-9 * (double)t.tv_nsec;
}

// covered pixels per second for `dabs` 30-px dabs on a 2048 x 1536 layer of `format`
double dab_throughput(PaintFormat format, int32_t bands, const PaintDabParams* params, int32_t dabs) {
    TestLayer layer(2048, 1536, 64, format, PAINT_ELEM_F32, true, bands);
    float* rows = (float*)calloc((size_t)dabs * PAINT_DAB_COLUMNS, sizeof(float));
    for (int32_t i = 0; i < dabs; i++) {
        float* row = rows + i * PAINT_DAB_COLUMNS;
        dab_row(row, 40.0f + (float)((i * 37) % 1960), 40.0f + (float)((i * 53) % 1450), 30.0f);
        row[PAINT_DAB_OPACITY] = 0.3f; row[PAINT_DAB_AA] = 1.0f; row[PAINT_DAB_PROFILE] = (float)PAINT_PROFILE_MYPAINT;
        row[PAINT_DAB_HARDNESS] = 0.8f;
        if (bands > 0) { row[PAINT_DAB_PIGMENT] = (float)(i % 2); row[PAINT_DAB_AMOUNT] = 0.5f; row[PAINT_DAB_THICKNESS] = 0.5f; }
    }
    double start = seconds_now();
    PaintResult r = paint_dabs(&layer.tiles, rows, dabs, PAINT_DAB_COLUMNS, params);
    double elapsed = seconds_now() - start;
    free(rows);
    EXPECT_EQ(r.status, PAINT_OK);
    double covered = (double)dabs * M_PI * 30.0 * 30.0;
    return covered / elapsed;
}

}  // namespace

TEST(PaintEngine, DISABLED_ThroughputMeetsTheBudgets) {
    PaintDabParams plain = PLAIN;
    plain.work_budget = UINT64_MAX / 4;
    double rgba = dab_throughput(PAINT_FORMAT_RGBA, 0, &plain, 20000);
    float yellow[3] = {0.9f, 0.75f, 0.05f}, blue[3] = {0.05f, 0.4f, 0.8f}, table[12];
    km_pigment(yellow, table); km_pigment(blue, table + 6);
    PaintDabParams pigment = pigment_params(table, 2);
    pigment.work_budget = UINT64_MAX / 4;
    double film = dab_throughput(PAINT_FORMAT_PIGMENT, 3, &pigment, 20000);
    RecordProperty("rgba_mpx_per_s", (int)(rgba / 1e6));
    RecordProperty("pigment_mpx_per_s", (int)(film / 1e6));
    GTEST_LOG_(INFO) << "dab throughput: rgba " << rgba / 1e6 << " M px/s, pigment (B = 3) " << film / 1e6 << " M px/s";
    EXPECT_GE(rgba, 200e6);
    EXPECT_GE(film, 50e6);
}
