#pragma once

#include "render.hpp"

// Shared contour expansion is used by transformed and rounded paths.
void render_path_append_rounded_rect_contour(RdtPath* path, Rect rect,
                                             const Corner* radius, bool clockwise);
