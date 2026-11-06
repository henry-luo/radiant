#include <gtest/gtest.h>
#include "../radiant/render.hpp"

TEST(RadiantSvgLengthTest, SelectsAxisForGeometryAttributes) {
    EXPECT_EQ(dom_svg_length_axis("x"), SVG_LENGTH_X);
    EXPECT_EQ(dom_svg_length_axis("x2"), SVG_LENGTH_X);
    EXPECT_EQ(dom_svg_length_axis("width"), SVG_LENGTH_X);
    EXPECT_EQ(dom_svg_length_axis("y"), SVG_LENGTH_Y);
    EXPECT_EQ(dom_svg_length_axis("cy"), SVG_LENGTH_Y);
    EXPECT_EQ(dom_svg_length_axis("height"), SVG_LENGTH_Y);
    EXPECT_EQ(dom_svg_length_axis("r"), SVG_LENGTH_DIAGONAL);
    EXPECT_EQ(dom_svg_length_axis("stroke-width"), SVG_LENGTH_DIAGONAL);
}
