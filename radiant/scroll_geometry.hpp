#pragma once

// Compute the native scrollable extent, including overflowing descendants.
extern "C" float dom_scrollable_extent(void* element, bool width_axis);
