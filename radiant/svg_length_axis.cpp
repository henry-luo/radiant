#include "render.hpp"
#include <string.h>

SvgLengthAxis dom_svg_length_axis(const char* name) {
    if (strcmp(name, "x") == 0 || strcmp(name, "x1") == 0 || strcmp(name, "x2") == 0 ||
        strcmp(name, "cx") == 0 || strcmp(name, "width") == 0 || strcmp(name, "rx") == 0) return SVG_LENGTH_X;
    if (strcmp(name, "y") == 0 || strcmp(name, "y1") == 0 || strcmp(name, "y2") == 0 ||
        strcmp(name, "cy") == 0 || strcmp(name, "height") == 0 || strcmp(name, "ry") == 0) return SVG_LENGTH_Y;
    return SVG_LENGTH_DIAGONAL;
}

