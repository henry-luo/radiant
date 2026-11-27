#pragma once

struct NativeGlContext;
struct NativeGlPresentFrame;

bool radiant_mac_present(void* cocoa_window, void* ns_gl_context, NativeGlContext* presenter,
                         NativeGlPresentFrame* frame);
void radiant_mac_present_destroy(void* cocoa_window, void* ns_gl_context, NativeGlContext* presenter);
