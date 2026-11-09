#pragma once

// PE6 (Radiant_Design_Paint_Engine.md): native pen capture beside the GLFW backend. macOS reads
// AppKit's tablet data; the other desktop platforms report a mouse until their own capture lands
// (PE-O4).

#include "event.hpp"

typedef struct RadiantWindow RadiantWindow;

// start or stop pen capture for a window whose input callbacks are registered.
void radiant_pen_enable(RadiantWindow* window, bool enabled);
// fill the pen fields of a pointer event built from the platform event being dispatched; a
// mouse event is left unchanged.
void radiant_pen_fill(MousePositionEvent* event);
