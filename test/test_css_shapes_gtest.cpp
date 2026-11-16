#include <gtest/gtest.h>
#include "radiant/shape_exclusion.hpp"
#include <math.h>

TEST(CssShapes, CircleUsesWholeLineBand) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_CIRCLE;
    shape.circle = {50.0f, 50.0f, 50.0f};
    float left, right;
    ASSERT_TRUE(shape_exclusion_band(&shape, 0.0f, 10.0f, 0.0f, &left, &right));
    EXPECT_NEAR(left, 20.0f, 0.0001f);
    EXPECT_NEAR(right, 80.0f, 0.0001f);
    ASSERT_TRUE(shape_exclusion_band(&shape, 40.0f, 60.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 0.0f);
    EXPECT_FLOAT_EQ(right, 100.0f);
    EXPECT_FALSE(shape_exclusion_band(&shape, 100.0f, 120.0f, 0.0f, &left, &right));
}

TEST(CssShapes, CircleMarginExpandsTheContour) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_CIRCLE;
    shape.circle = {50.0f, 50.0f, 30.0f};
    float left, right;
    EXPECT_FALSE(shape_exclusion_band(&shape, 0.0f, 10.0f, 0.0f, &left, &right));
    ASSERT_TRUE(shape_exclusion_band(&shape, 0.0f, 20.0f, 20.0f, &left, &right));
    EXPECT_NEAR(left, 10.0f, 0.0001f);
    EXPECT_NEAR(right, 90.0f, 0.0001f);
}

TEST(CssShapes, PolygonIncludesInteriorVerticesAndSlopedMargins) {
    float x[] = {0.0f, 100.0f, 0.0f};
    float y[] = {0.0f, 50.0f, 100.0f};
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_POLYGON;
    shape.polygon = {lam::own_arr(x), lam::own_arr(y), 3};
    float left, right;
    ASSERT_TRUE(shape_exclusion_band(&shape, 40.0f, 60.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 0.0f);
    EXPECT_FLOAT_EQ(right, 100.0f);
    ASSERT_TRUE(shape_exclusion_band(&shape, 10.0f, 20.0f, 5.0f, &left, &right));
    EXPECT_NEAR(left, -5.0f, 0.0001f);
    EXPECT_NEAR(right, 40.0f + 5.0f * sqrtf(5.0f), 0.001f);
}

TEST(CssShapes, EllipseMarginIsADistanceOffset) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_ELLIPSE;
    shape.ellipse = {50.0f, 50.0f, 40.0f, 20.0f};
    float left, right;
    ASSERT_TRUE(shape_exclusion_band(&shape, 45.0f, 55.0f, 10.0f, &left, &right));
    EXPECT_NEAR(left, 0.0f, 0.0001f);
    EXPECT_NEAR(right, 100.0f, 0.0001f);
    ASSERT_TRUE(shape_exclusion_band(&shape, 20.0f, 24.636708f, 10.0f, &left, &right));
    // analytic witness: ellipse point (74,34) plus ten times its unit normal.
    EXPECT_NEAR(right, 77.51123f, 0.001f);
    EXPECT_NEAR(left + right, 100.0f, 0.0001f);
}

TEST(CssShapes, InsetDegenerateEdgesAndRoundedMargin) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_INSET;
    shape.inset = {50.0f, 0.0f, 0.0f, 100.0f, 0.0f, 0.0f};
    float left, right;
    ASSERT_TRUE(shape_exclusion_band(&shape, 0.0f, 10.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 50.0f);
    EXPECT_FLOAT_EQ(right, 50.0f);
    shape.inset = {0.0f, 50.0f, 100.0f, 0.0f, 0.0f, 0.0f};
    EXPECT_FALSE(shape_exclusion_band(&shape, 40.0f, 60.0f, 0.0f, &left, &right));
    ASSERT_TRUE(shape_exclusion_band(&shape, 40.0f, 45.0f, 10.0f, &left, &right));
    EXPECT_NEAR(left, -sqrtf(75.0f), 0.0001f);
    EXPECT_NEAR(right, 100.0f + sqrtf(75.0f), 0.0001f);
}

TEST(CssShapes, RoundedBoxUsesCornersAndNormalizesOverlaps) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_ROUNDED_RECT;
    shape.rounded_rect = {0.0f, 0.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f, 100.0f};
    float left, right;
    ASSERT_TRUE(shape_exclusion_band(&shape, 0.0f, 10.0f, 0.0f, &left, &right));
    EXPECT_NEAR(left, 20.0f, 0.001f);
    EXPECT_NEAR(right, 80.0f, 0.001f);
    ASSERT_TRUE(shape_exclusion_band(&shape, -10.0f, 0.0f, 10.0f, &left, &right));
    EXPECT_NEAR(left, 50.0f - sqrtf(1100.0f), 0.001f);
    EXPECT_NEAR(right, 50.0f + sqrtf(1100.0f), 0.001f);
}

TEST(CssShapes, ConcavePolygonBandIncludesDisjointLobes) {
    float x[] = {0, 100, 100, 20, 20, 100, 100, 0};
    float y[] = {0, 0, 20, 20, 80, 80, 100, 100};
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_POLYGON;
    shape.polygon = {lam::own_arr(x), lam::own_arr(y), 8};
    float left, right;
    ASSERT_TRUE(shape_exclusion_band(&shape, 30.0f, 70.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 0.0f);
    EXPECT_FLOAT_EQ(right, 20.0f);
    ASSERT_TRUE(shape_exclusion_band(&shape, 10.0f, 90.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(right, 100.0f);
}

TEST(CssShapes, InvalidQueriesDoNotPublishBounds) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_INSET;
    shape.inset = {0.0f, 0.0f, 100.0f, 100.0f, 0.0f, 0.0f};
    float left = 123.0f, right = 456.0f;
    EXPECT_FALSE(shape_exclusion_band(&shape, 10.0f, 10.0f, 0.0f, &left, &right));
    EXPECT_FALSE(shape_exclusion_band(&shape, 0.0f, 10.0f, -1.0f, &left, &right));
    EXPECT_FALSE(shape_exclusion_band(&shape, NAN, 10.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 123.0f);
    EXPECT_FLOAT_EQ(right, 456.0f);
}

TEST(CssShapes, ImageAlphaThresholdIsStrictAndRespectsRowStride) {
    uint8_t pixels[60] = {};
    pixels[7] = 128;
    pixels[11] = 255;
    pixels[35] = 255;
    pixels[43] = 64;
    pixels[19] = 255; // row padding must not become an image pixel
    ImagePixelView image = {pixels, sizeof(pixels), 20, 4, 3, 4, false};
    ShapeImageRow rows[3];
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 0.0f, rows, 3));
    EXPECT_FLOAT_EQ(rows[0].left, 0.25f);
    EXPECT_FLOAT_EQ(rows[0].right, 0.75f);
    EXPECT_FLOAT_EQ(rows[1].left, 0.75f);
    EXPECT_FLOAT_EQ(rows[1].right, 1.0f);
    EXPECT_FLOAT_EQ(rows[2].left, 0.0f);
    EXPECT_FLOAT_EQ(rows[2].right, 0.25f);
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 128.0f / 255.0f, rows, 3));
    EXPECT_FLOAT_EQ(rows[0].left, 0.5f);
    EXPECT_FLOAT_EQ(rows[0].right, 0.75f);
    EXPECT_GT(rows[2].left, rows[2].right);
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 1.0f, rows, 3));
    for (const ShapeImageRow& row : rows) EXPECT_GT(row.left, row.right);
}

TEST(CssShapes, ImageRowsScaleToTheUsedRectangleAndExpandByDistance) {
    ShapeImageRow rows[] = {{0.25f, 0.75f}, {0.75f, 1.0f}, {1.0f, 0.0f}};
    Rect rectangle = {10.0f, 20.0f, 80.0f, 60.0f};
    float left, right;
    ASSERT_TRUE(shape_exclusion_image_band(rows, 3, rectangle, 20.0f, 30.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 30.0f);
    EXPECT_FLOAT_EQ(right, 70.0f);
    ASSERT_TRUE(shape_exclusion_image_band(rows, 3, rectangle, 30.0f, 50.0f, 0.0f, &left, &right));
    EXPECT_FLOAT_EQ(left, 30.0f);
    EXPECT_FLOAT_EQ(right, 90.0f);
    ASSERT_TRUE(shape_exclusion_image_band(rows, 3, rectangle, 0.0f, 15.0f, 10.0f, &left, &right));
    EXPECT_NEAR(left, 30.0f - sqrtf(75.0f), 0.0001f);
    EXPECT_NEAR(right, 70.0f + sqrtf(75.0f), 0.0001f);
    EXPECT_FALSE(shape_exclusion_image_band(rows, 3, rectangle, 60.0f, 80.0f, 0.0f, &left, &right));
}

TEST(CssShapes, AlphaRowsOwnNoPixelsAndCanBeRebuiltAfterMutation) {
    uint8_t pixels[] = {0, 0, 0, 0, 0, 0, 0, 255};
    ImagePixelView image = {pixels, sizeof(pixels), 8, 2, 1, 4, true};
    ShapeImageRow row;
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 0.5f, &row, 1));
    pixels[3] = 255;
    pixels[7] = 0;
    EXPECT_FLOAT_EQ(row.left, 0.5f);
    EXPECT_FLOAT_EQ(row.right, 1.0f);
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 0.5f, &row, 1));
    EXPECT_FLOAT_EQ(row.left, 0.0f);
    EXPECT_FLOAT_EQ(row.right, 0.5f);
}

TEST(CssShapes, AlphaRowsValidateBufferExtentBeforePublishing) {
    uint8_t pixels[8] = {};
    ImagePixelView image = {pixels, 7, 4, 1, 2, 4, false};
    ShapeImageRow rows[] = {{123.0f, 456.0f}, {123.0f, 456.0f}};
    EXPECT_FALSE(shape_exclusion_alpha_rows(&image, 0.0f, rows, 2));
    EXPECT_FLOAT_EQ(rows[0].left, 123.0f);
    image.capacity = sizeof(pixels);
    EXPECT_FALSE(shape_exclusion_alpha_rows(&image, 0.0f, rows, 1));
    EXPECT_FALSE(shape_exclusion_alpha_rows(&image, NAN, rows, 2));
    EXPECT_FLOAT_EQ(rows[0].right, 456.0f);
}

TEST(CssShapes, OpaqueImagesAndClampedThresholds) {
    uint8_t pixels[] = {0, 0, 0};
    ImagePixelView image = {pixels, sizeof(pixels), 3, 1, 1, 3, false};
    ShapeImageRow row;
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, -10.0f, &row, 1));
    EXPECT_FLOAT_EQ(row.left, 0.0f);
    EXPECT_FLOAT_EQ(row.right, 1.0f);
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 10.0f, &row, 1));
    EXPECT_GT(row.left, row.right);
    image.channels = 1;
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 0.0f, &row, 1));
    EXPECT_FLOAT_EQ(row.left, 0.0f);
    EXPECT_FLOAT_EQ(row.right, 1.0f);
}

TEST(CssShapes, RoundedInsetDegenerateHeightStillExpandsWithMargin) {
    ClipShape shape = {};
    shape.type = CLIP_SHAPE_ROUNDED_RECT;
    shape.rounded_rect = {0.0f, 50.0f, 100.0f, 0.0f, 10.0f, 10.0f, 10.0f, 10.0f};
    float left, right;
    EXPECT_FALSE(shape_exclusion_band(&shape, 40.0f, 60.0f, 0.0f, &left, &right));
    ASSERT_TRUE(shape_exclusion_band(&shape, 40.0f, 45.0f, 10.0f, &left, &right));
    EXPECT_NEAR(left, -sqrtf(75.0f), 0.0001f);
    EXPECT_NEAR(right, 100.0f + sqrtf(75.0f), 0.0001f);
}

TEST(CssShapes, AlphaRowsMapPhysicalPixelsToReversedVerticalAxes) {
    uint8_t bytes[] = {
        0,0,0,255, 0,0,0,0,
        0,0,0,0, 0,0,0,255,
        0,0,0,0, 0,0,0,0
    };
    ImagePixelView image = {bytes, sizeof(bytes), 8, 2, 3, 4, false};
    ShapeImageRow rows[2];
    EXPECT_FALSE(shape_exclusion_alpha_rows(&image, 0.0f, rows, 1, true, true, true));
    ASSERT_TRUE(shape_exclusion_alpha_rows(&image, 0.0f, rows, 2, true, true, true));
    EXPECT_FLOAT_EQ(rows[0].left, 1.0f / 3.0f);
    EXPECT_FLOAT_EQ(rows[0].right, 2.0f / 3.0f);
    EXPECT_FLOAT_EQ(rows[1].left, 2.0f / 3.0f);
    EXPECT_FLOAT_EQ(rows[1].right, 1.0f);
}

TEST(CssShapes, ShapeMarginQueriesTheWholeLineEvenWhenTheReferenceBoxIsShorter) {
    Rect rect = {150.0f, 95.0f, 15.0f, 10.0f};
    Corner radius = {};
    float left, right;
    ASSERT_TRUE(shape_exclusion_box_band(rect, &radius, 20.0f, 40.0f, 70.0f, &left, &right));
    // the line bottom is 55px above the rectangle, so its distance offset reaches sqrt(70^2-55^2).
    EXPECT_NEAR(left, 150.0f - sqrtf(70.0f * 70.0f - 55.0f * 55.0f), 0.0001f);
    ASSERT_TRUE(shape_exclusion_box_band(rect, &radius, 20.0f, 30.0f, 70.0f, &left, &right));
    EXPECT_NEAR(left, 150.0f - sqrtf(70.0f * 70.0f - 65.0f * 65.0f), 0.0001f);
}
