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
