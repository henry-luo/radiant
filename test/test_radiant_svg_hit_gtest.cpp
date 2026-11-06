#include <gtest/gtest.h>
#include "../radiant/svg_hit.hpp"

TEST(MapGeometry, SelfCrossingFillBoundariesAndRectanglesDoNotDependOnSignedArea) {
    RdtPath* path=rdt_path_new();ASSERT_NE(path,nullptr);
    rdt_path_move_to(path,32,32);rdt_path_line_to(path,224,160);rdt_path_line_to(path,32,160);
    rdt_path_line_to(path,224,32);rdt_path_close(path);
    EXPECT_TRUE(dom_geometry_path_query(path,128,48,128,48,false,RDT_FILL_EVEN_ODD,-1,0,0));
    EXPECT_TRUE(dom_geometry_path_query(path,80,64,80,64,false,RDT_FILL_EVEN_ODD,-1,0,0));
    EXPECT_TRUE(dom_geometry_path_query(path,75,62,81,64,true,RDT_FILL_EVEN_ODD,-1,0,0));
    EXPECT_FALSE(dom_geometry_path_query(path,32,80,40,88,true,RDT_FILL_EVEN_ODD,-1,0,0));
    rdt_path_free(path);path=rdt_path_new();ASSERT_NE(path,nullptr);
    rdt_path_move_to(path,32,32);rdt_path_line_to(path,96,96);rdt_path_line_to(path,160,160);rdt_path_close(path);
    EXPECT_FALSE(dom_geometry_path_query(path,90,90,100,100,true,RDT_FILL_EVEN_ODD,-1,0,0));
    rdt_path_free(path);
}

TEST(RadiantSvgHitTest, CurvedFillAndOpenStrokeUseNativeContours) {
    RdtPath* curve = rdt_path_new();
    ASSERT_NE(curve, nullptr);
    rdt_path_move_to(curve, 0, 0);
    rdt_path_cubic_to(curve, 0, 10, 10, 10, 10, 0);
    rdt_path_close(curve);
    EXPECT_TRUE(dom_geometry_path_query(curve, 5, 3, 5, 3, false,
                                         RDT_FILL_WINDING, -1, 0, 0));
    EXPECT_FALSE(dom_geometry_path_query(curve, 5, 9, 5, 9, false,
                                          RDT_FILL_WINDING, -1, 0, 0));
    rdt_path_free(curve);

    RdtPath* line = rdt_path_new();
    ASSERT_NE(line, nullptr);
    rdt_path_move_to(line, 0, 0);
    rdt_path_line_to(line, 100, 0);
    EXPECT_TRUE(dom_geometry_path_query(line, 50, 0.5f, 50, 0.5f, false,
                                         -1, 2, RDT_CAP_BUTT, RDT_JOIN_MITER));
    EXPECT_FALSE(dom_geometry_path_query(line, 50, 2, 50, 2, false,
                                          -1, 2, RDT_CAP_BUTT, RDT_JOIN_MITER));
    rdt_path_free(line);
}

