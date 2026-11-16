#include <gtest/gtest.h>
#include "radiant/shape_exclusion.hpp"
#include "radiant/layout.hpp"
#include "radiant/render.hpp"
#include "lib/file.h"
#include <stdio.h>

class CssShapesLayout : public ::testing::Test {
protected:
    RenderExportSession session = {};
    void TearDown() override { render_export_session_finish(&session); }
    bool compose(const char* declaration, const char* side = "left", const char* extra = "",
            const char* before_text = "") {
        char html[8192];
        snprintf(html, sizeof(html),
            "<!doctype html><html><head><style>html,body{margin:0}"
            "#flow{width:300px;font:20px/20px monospace}"
            "#float{float:%s;width:100px;height:100px;%s}%s"
            "</style></head><body><div id='flow'><div id='float'></div>%s"
            "<span id='text'>one two three four five six seven eight nine ten "
            "one two three four five six seven eight nine ten "
            "one two three four five six seven eight nine ten "
            "one two three four five six seven eight nine ten</span>"
            "<div id='clear' style='clear:both;height:10px'></div>"
            "</div></body></html>", side, declaration, extra, before_text);
        if (file_ensure_dir("temp") != 0 ||
            write_text_file_atomic("temp/css-shapes-layout.html", html) != 0) return false;
        return render_export_session_begin(&session, "temp/css-shapes-layout.html",
            300, 300, 300, 300, 1.0f, false, false, 1.0f, false);
    }
    DomElement* element(const char* id) {
        return dom_find_element_by_id(session.document->root, id);
    }
    TextRect* first_text_rect() {
        DomElement* text = element("text");
        return text && text->first_child && text->first_child->is_text()
            ? text->first_child->as_text()->rect.get() : nullptr;
    }
    float text_x(TextRect* rect) {
        // text rectangles use the containing block's frame, including through inline wrappers.
        return view_geometry_local_to_block_document(element("text")->first_child,
            {rect->x, rect->y}).x;
    }
    bool write_alpha_image() {
        uint8_t bytes[] = {0, 0, 0, 255, 0, 0, 0, 128, 0, 0, 0, 0, 0, 0, 0, 0};
        ImagePixelView pixels = {bytes, sizeof(bytes), sizeof(bytes), 4, 1, 4, false};
        ImageEncodeOptions options;
        image_encode_options_init(&options, IMAGE_CODEC_PNG);
        ImageEncoded encoded = {};
        if (image_encode_to_memory(nullptr, &pixels, &options, &encoded, nullptr) != IMAGE_OK) return false;
        file_ensure_dir("temp");
        bool success = write_binary_file_atomic("temp/shape-alpha.png", encoded.data, encoded.length) == 0;
        image_free(encoded.data);
        return success;
    }
    void restyle(const char* declaration) {
        EXPECT_GT(dom_element_apply_inline_style(element("float"), declaration), 0);
        layout_html_doc(session.ui_context, session.document, false);
    }
};

TEST_F(CssShapesLayout, CircleChangesActualTextLineStarts) {
    ASSERT_TRUE(compose("shape-outside:circle(50%);"));
    TextRect* rect = first_text_rect();
    ASSERT_NE(rect, nullptr);
    EXPECT_NEAR(text_x(rect), 90.0f, 0.02f);
    ASSERT_NE(rect->next, nullptr);
    EXPECT_NEAR(text_x(rect->next), 50.0f + sqrtf(2400.0f), 0.02f);
    EXPECT_NEAR(element("float")->width, 100.0f, 0.001f);
    EXPECT_GE(element("clear")->y, 100.0f);
}

TEST_F(CssShapesLayout, CircleInOffsetBfcUsesItsOwnBorderCoordinateFrame) {
    ASSERT_TRUE(compose("shape-outside:circle(50%) border-box;", "left",
        "#flow{display:flow-root;margin-left:70px;margin-top:40px;padding:20px;border:3px solid}"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 183.0f, 0.02f);
}

TEST_F(CssShapesLayout, RectangularFloatInOffsetBfcUsesTheSameFrame) {
    ASSERT_TRUE(compose("shape-outside:none;", "left",
        "#flow{display:flow-root;margin-left:70px;margin-top:40px;padding:20px;border:3px solid}"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 193.0f, 0.02f);
}

TEST_F(CssShapesLayout, NegativeOffsetFloatKeepsItsExclusionOnRefresh) {
    ASSERT_TRUE(compose("shape-outside:none;", "left", "#flow{margin-left:-200px}"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), -100.0f, 0.02f);
}

TEST_F(CssShapesLayout, CircleInGridAssignedBfcUsesItsOwnFrame) {
    ASSERT_TRUE(compose("shape-outside:circle(50%) border-box;", "left",
        "body{display:grid;grid-template-columns:340px;padding:40px}"
        "#flow{display:flow-root;padding:20px;border:3px solid}"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 153.0f, 0.02f);
}

TEST_F(CssShapesLayout, RectangularFloatInGridAssignedBfcUsesItsOwnFrame) {
    ASSERT_TRUE(compose("shape-outside:none;", "left",
        "body{display:grid;grid-template-columns:340px;padding:40px}"
        "#flow{display:flow-root;padding:20px;border:3px solid}"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 163.0f, 0.02f);
}

TEST_F(CssShapesLayout, CircleInFlexAssignedBfcUsesItsOwnFrame) {
    ASSERT_TRUE(compose("shape-outside:circle(50%) border-box;", "left",
        "body{display:flex;padding:40px}"
        "#flow{display:flow-root;flex:none;padding:20px;border:3px solid}"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 153.0f, 0.02f);
}

TEST_F(CssShapesLayout, AtomicInlinesWrapTheShapeInsteadOfAvoidingTheFloatBox) {
    ASSERT_TRUE(compose("shape-outside:border-box;", "left",
        "#flow{width:200px;font-size:0;line-height:0}#text{display:none}"
        "#float{width:25px;height:25px;padding:25px;border:25px solid;margin:25px}",
        "<div id='wide' style='display:inline-block;width:200px;height:25px'></div>"
        "<div id='narrow' style='display:inline-block;width:25px;height:25px'></div>"));
    EXPECT_FLOAT_EQ(element("float")->x, 25.0f);
    EXPECT_FLOAT_EQ(element("float")->y, 25.0f);
    EXPECT_FLOAT_EQ(element("wide")->x, 0.0f);
    EXPECT_FLOAT_EQ(element("wide")->y, 0.0f);
    EXPECT_FLOAT_EQ(element("narrow")->x, 150.0f);
    EXPECT_FLOAT_EQ(element("narrow")->y, 25.0f);
}

TEST_F(CssShapesLayout, TallerPendingAtomicDoesNotWidenThePreviousLineBand) {
    ASSERT_TRUE(compose("shape-outside:circle(closest-side at center) border-box;", "right",
        "#flow{width:200px;direction:rtl;font-size:0;line-height:0}#text{display:none}"
        "#float{width:40px;height:40px;padding:20px;border:20px solid;margin-left:10px}",
        "<div id='short1' style='display:inline-block;width:60px;height:12px'></div>"
        "<div id='short2' style='display:inline-block;width:60px;height:12px'></div>"
        "<div id='tall' style='display:inline-block;width:60px;height:36px'></div>"));
    EXPECT_NEAR(element("short1")->x, 44.0f, 0.02f);
    EXPECT_FLOAT_EQ(element("short1")->y, 0.0f);
    EXPECT_NEAR(element("short2")->x, 32.0f, 0.02f);
    EXPECT_FLOAT_EQ(element("short2")->y, 12.0f);
    EXPECT_NEAR(element("tall")->x, 20.0f, 0.02f);
    EXPECT_FLOAT_EQ(element("tall")->y, 24.0f);
}

TEST_F(CssShapesLayout, SvgImageAlphaUsesTheSharedRasterizer) {
    ASSERT_EQ(file_ensure_dir("temp"), 0);
    ASSERT_EQ(write_text_file_atomic("temp/shape-alpha.svg",
        "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100'>"
        "<rect width='50' height='100' fill='green' fill-opacity='.2'/></svg>"), 0);
    ASSERT_TRUE(compose("shape-outside:url(shape-alpha.svg);shape-image-threshold:.1;"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 50.0f, 0.02f);
    restyle("shape-image-threshold:.3");
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 0.0f, 0.02f);
}

TEST_F(CssShapesLayout, VerticalAtomicBandsUsePhysicalBlockSize) {
    const char* modes[] = {"vertical-rl", "vertical-lr", "sideways-lr"};
    for (const char* mode : modes) {
        char extra[256];
        snprintf(extra, sizeof(extra),
            "#flow{writing-mode:%s;width:120px;height:300px}#text,#clear{display:none}", mode);
        ASSERT_TRUE(compose("shape-outside:circle(50%);", "left", extra,
            "<div id='probe' style='display:inline-block;width:20px;height:200px'></div>"));
        EXPECT_NEAR(element("probe")->y, strcmp(mode, "sideways-lr") == 0 ? 10.0f : 90.0f, 0.02f);
        render_export_session_finish(&session);
    }
}

TEST_F(CssShapesLayout, EllipticalInsetCornersReachAtomicLineGeometry) {
    ASSERT_TRUE(compose("shape-outside:inset(10px round 0 40px/0 60px) border-box;", "left",
        "#flow{width:200px;font-size:0;line-height:0}#text,#clear{display:none}#float{height:140px}",
        "<div id='wide-inset' style='display:inline-block;width:200px;height:10px'></div>"
        "<div id='corner' style='display:inline-block;width:80px;height:24px'></div>"
        "<div id='middle' style='display:inline-block;width:80px;height:36px'></div>"));
    EXPECT_FLOAT_EQ(element("wide-inset")->x, 0.0f);
    EXPECT_NEAR(element("corner")->x, 82.0f, 0.02f);
    EXPECT_FLOAT_EQ(element("corner")->y, 10.0f);
    EXPECT_FLOAT_EQ(element("middle")->x, 90.0f);
    EXPECT_FLOAT_EQ(element("middle")->y, 34.0f);
}

TEST_F(CssShapesLayout, GeneratedImageContoursRebuildWithGradientStyleChanges) {
    ASSERT_TRUE(compose("shape-outside:linear-gradient(to right,black 50%,transparent 50%);"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 50.0f, 0.02f);
    restyle("shape-outside:radial-gradient(circle at 50px 50px,black 30px,transparent 30px)");
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 0.0f, 0.02f);
    ASSERT_NE(first_text_rect()->next, nullptr);
    EXPECT_NEAR(text_x(first_text_rect()->next), 78.0f, 0.02f);
}

TEST_F(CssShapesLayout, ZeroLineHeightFloatWrapperKeepsItsFontBoxBesideAtomicContent) {
    ASSERT_TRUE(compose("shape-outside:none;display:none;", "left",
        "#flow{width:200px;font:16px/0 monospace}#text,#clear{display:none}",
        "<span id='wrapper' style='direction:rtl;unicode-bidi:bidi-override'>"
        "<div style='float:left;width:100px;height:100px;shape-outside:circle(50%)'></div>"
        "</span><div id='probe' style='display:inline-block;width:60px;height:12px'></div>"));
    float font_height = font_get_cell_height(element("wrapper")->fontp()->font_handle);
    EXPECT_NEAR(element("wrapper")->height, font_height, 0.02f);
    EXPECT_FLOAT_EQ(element("wrapper")->width, 0.0f);
    EXPECT_FLOAT_EQ(element("probe")->y, 0.0f);
}

TEST_F(CssShapesLayout, NoneKeepsRectangularFloatWrapping) {
    ASSERT_TRUE(compose("shape-outside:none;shape-margin:40px;"));
    TextRect* rect = first_text_rect();
    ASSERT_NE(rect, nullptr);
    EXPECT_NEAR(text_x(rect), 100.0f, 0.02f);
}

TEST_F(CssShapesLayout, VariablesAndPercentageMarginReachLineGeometry) {
    ASSERT_TRUE(compose("--radius:30px;shape-outside:circle(var(--radius));shape-margin:calc(5% + 5px);"));
    TextRect* rect = first_text_rect();
    ASSERT_NE(rect, nullptr);
    EXPECT_NEAR(text_x(rect), 90.0f, 0.02f);
    char value[128];
    ASSERT_TRUE(css_prop_serialize_computed(element("float"), CSS_PROPERTY_SHAPE_IMAGE_THRESHOLD, 0, value, sizeof(value)));
    EXPECT_STREQ(value, "0");
}

TEST_F(CssShapesLayout, RightFloatTriangleUsesTheOppositeContourEdge) {
    ASSERT_TRUE(compose("shape-outside:polygon(100% 0, 100% 100%, 0 100%);", "right"));
    TextRect* rect = first_text_rect();
    ASSERT_NE(rect, nullptr);
    // line zero has 280px available; it fits more source text than the 200px margin box.
    EXPECT_GT(rect->width, 200.0f);
    EXPECT_LE(rect->width, 280.02f);
}

TEST_F(CssShapesLayout, ShapeImageThresholdAndMarginChangeActualLineStarts) {
    ASSERT_TRUE(write_alpha_image());
    ASSERT_TRUE(compose("shape-outside:url('shape-alpha.png');shape-image-threshold:0;"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 50.0f, 0.02f);
    restyle("shape-image-threshold:75%;shape-margin:10px");
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 35.0f, 0.02f);
    char value[128];
    ASSERT_TRUE(css_prop_serialize_computed(element("float"), CSS_PROPERTY_SHAPE_IMAGE_THRESHOLD, 0, value, sizeof(value)));
    EXPECT_STREQ(value, "0.75");
    restyle("shape-image-threshold:2;shape-margin:0");
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 0.0f, 0.02f);
}

TEST_F(CssShapesLayout, RetainedShapeChangesRebuildContours) {
    ASSERT_TRUE(compose("shape-outside:circle(50%);"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 90.0f, 0.02f);
    restyle("shape-outside:inset(0 50% 0 0)");
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 50.0f, 0.02f);
    restyle("shape-outside:none");
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 100.0f, 0.02f);
}

TEST_F(CssShapesLayout, BoxShapeIncludesBorderRadius) {
    ASSERT_TRUE(compose("shape-outside:border-box;border-radius:50%;"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 90.0f, 0.02f);
}

TEST_F(CssShapesLayout, OversizedShapeAndMarginAreClippedToMarginBox) {
    ASSERT_TRUE(compose("shape-outside:circle(500px);shape-margin:50px;"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 100.0f, 0.02f);
}

TEST_F(CssShapesLayout, EllipseLeavesLinesAboveTheContourUnshortened) {
    ASSERT_TRUE(compose("shape-outside:ellipse(50px 20px);"));
    TextRect* rect = first_text_rect();
    ASSERT_NE(rect, nullptr);
    EXPECT_NEAR(text_x(rect), 0.0f, 0.02f);
    ASSERT_NE(rect->next, nullptr);
    EXPECT_NEAR(text_x(rect->next), 50.0f + sqrtf(1875.0f), 0.02f);
}

TEST_F(CssShapesLayout, ContentReferenceBoxExcludesBorderPaddingAndMargin) {
    ASSERT_TRUE(compose("shape-outside:inset(0) content-box;box-sizing:border-box;"
        "border:10px solid;padding:10px;margin:10px;"));
    TextRect* rect = first_text_rect();
    ASSERT_NE(rect, nullptr);
    EXPECT_NEAR(text_x(rect), 0.0f, 0.02f);
    ASSERT_NE(rect->next, nullptr);
    EXPECT_NEAR(text_x(rect->next), 90.0f, 0.02f);
}

TEST_F(CssShapesLayout, InvalidShapeDoesNotDisplaceAnEarlierValidDeclaration) {
    ASSERT_TRUE(compose("shape-outside:circle(50%);shape-outside:circle(-1px);shape-margin:-20px;"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 90.0f, 0.02f);
}

TEST_F(CssShapesLayout, FollowingFloatsStillPackByTheirMarginBoxes) {
    ASSERT_TRUE(compose("shape-outside:circle(50%);", "left", "",
        "<div id='second' style='float:left;width:100px;height:100px'></div>"));
    ASSERT_NE(element("second"), nullptr);
    EXPECT_NEAR(element("second")->x, 100.0f, 0.02f);
    EXPECT_NEAR(element("second")->y, element("float")->y, 0.02f);
}

TEST_F(CssShapesLayout, IndependentBlocksStillAvoidTheFloatMarginBox) {
    ASSERT_TRUE(compose("shape-outside:circle(50%);", "left", "",
        "<div id='avoid' style='overflow:hidden;height:20px'></div>"));
    ASSERT_NE(element("avoid"), nullptr);
    EXPECT_NEAR(element("avoid")->x, 100.0f, 0.02f);
    EXPECT_NEAR(element("avoid")->width, 200.0f, 0.02f);
}

TEST_F(CssShapesLayout, RemoteImageNeedsCorsAdmissionBeforeAlphaExtraction) {
    ASSERT_TRUE(compose("shape-outside:url('https://example.invalid/shape.png');"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 100.0f, 0.02f);
}

TEST_F(CssShapesLayout, TransparentDataImageHasNoExclusion) {
    ASSERT_TRUE(compose("shape-outside:url('data:image/png;base64,"
        "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAAC0lEQVR4nGNgAAIAAAUAAXpeqz8AAAAASUVORK5CYII=');"));
    ASSERT_NE(first_text_rect(), nullptr);
    EXPECT_NEAR(text_x(first_text_rect()), 0.0f, 0.02f);
}

TEST(CssShapesFloatQueries, PlacementAndBfcAvoidanceKeepMarginBoxes) {
    ShapeFloatContour contour = {};
    contour.shape.type = CLIP_SHAPE_CIRCLE;
    contour.shape.circle = {50.0f, 50.0f, 50.0f};
    FloatBox floating = {};
    floating.margin_box_right = floating.margin_box_bottom = 100.0f;
    floating.shape_contour = lam::up(&contour);
    BlockContext context = {};
    context.left_floats = lam::up(&floating);
    context.left_float_count = 1;
    context.float_right_edge = 300.0f;
    context.lowest_float_bottom = 100.0f;
    FloatAvailableSpace line = block_context_space_at_y(&context, 0.0f, 20.0f, true);
    EXPECT_NEAR(line.left, 90.0f, 0.0001f);
    FloatAvailableSpace block = block_context_space_at_y(&context, 0.0f, 20.0f);
    EXPECT_FLOAT_EQ(block.left, 100.0f);
    FloatAvailableSpace placement = block_context_space_at_y(&context, 0.0f, 20.0f, false, true);
    EXPECT_FLOAT_EQ(placement.left, 100.0f);
}

TEST(CssShapesFloatQueries, LineFitFindsInteriorOpeningsBeforeTheFloatBottom) {
    ShapeFloatContour contour = {};
    contour.shape.type = CLIP_SHAPE_CIRCLE;
    contour.shape.circle = {0.0f, 0.0f, sqrtf(5000.0f)};
    FloatBox floating = {};
    floating.margin_box_right = floating.margin_box_bottom = 100.0f;
    floating.shape_contour = lam::up(&contour);
    BlockContext context = {};
    context.left_floats = lam::up(&floating);
    context.left_float_count = 1;
    context.float_right_edge = 300.0f;
    context.lowest_float_bottom = 100.0f;
    Linebox line = {};
    line.right = 300.0f;
    EXPECT_NEAR(find_line_y_for_width_with_floats(&context, &context, &line,
        250.0f, 0.0f, 20.0f), 50.0f, kTextLayoutSubpixelEpsilon);

    float xs[] = {0.0f, 100.0f, 0.0f};
    float ys[] = {0.0f, 0.0f, 100.0f};
    contour.shape = {};
    contour.shape.type = CLIP_SHAPE_POLYGON;
    contour.shape.polygon = {lam::own_arr(xs), lam::own_arr(ys), 3};
    EXPECT_NEAR(find_line_y_for_width_with_floats(&context, &context, &line,
        250.0f, 0.0f, 20.0f), 50.0f, kTextLayoutSubpixelEpsilon);

    // the earliest opening can close again before a midpoint or the float bottom.
    float concave_x[] = {0, 100, 100, 20, 20, 100, 100, 0};
    float concave_y[] = {0, 0, 40, 40, 49, 49, 100, 100};
    contour.shape.polygon = {lam::own_arr(concave_x), lam::own_arr(concave_y), 8};
    EXPECT_NEAR(find_line_y_for_width_with_floats(&context, &context, &line,
        250.0f, 0.0f, 5.0f), 40.0f, kTextLayoutSubpixelEpsilon);

    floating.shape_contour = nullptr;
    EXPECT_FLOAT_EQ(find_line_y_for_width_with_floats(&context, &context, &line,
        250.0f, 0.0f, 20.0f), 100.0f);
}
