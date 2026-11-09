// Radiant paint engine kernels (Radiant_Design_Paint_Engine PE3, PE4, PE7, PE11).
#include <gtest/gtest.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "radiant/paint/paint.hpp"

namespace {

// a fully allocated tile set over plain heap buffers, as the Lambda bridge supplies them.
struct TestLayer {
    PaintTiles tiles = {};
    PaintView* views = nullptr;
    TestLayer(int32_t width, int32_t height, int32_t tile, PaintFormat format, PaintElem elem, bool allocate = true) {
        tiles.width = width; tiles.height = height; tiles.tile_size = tile;
        tiles.tiles_x = (width + tile - 1) / tile; tiles.tiles_y = (height + tile - 1) / tile;
        tiles.format = format; tiles.elem = elem;
        int32_t channels = format == PAINT_FORMAT_RGBA ? 4 : 1;
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
