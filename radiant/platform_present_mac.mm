#import <AppKit/AppKit.h>
#import <CoreVideo/CVPixelBuffer.h>
#import <IOSurface/IOSurface.h>
#import <OpenGL/OpenGL.h>
#import <OpenGL/gl3.h>
#import <QuartzCore/QuartzCore.h>
#import <objc/runtime.h>

#include "gl_core.hpp"
#include "platform_present_mac.hpp"
#include "lib/log.h"

struct MacPresentBuffer {
    IOSurfaceRef surface = nullptr;
    GLuint texture = 0;
    GLuint framebuffer = 0;

    void release(bool context_current = true) {
        if (context_current && framebuffer) glDeleteFramebuffers(1, &framebuffer);
        if (context_current && texture) glDeleteTextures(1, &texture);
        if (surface) CFRelease(surface);
        *this = {};
    }

    bool prepare(CGLContextObj context, int width, int height) {
        NSDictionary* properties = @{
            (__bridge NSString*)kIOSurfaceWidth: @(width),
            (__bridge NSString*)kIOSurfaceHeight: @(height),
            (__bridge NSString*)kIOSurfaceBytesPerElement: @4,
            (__bridge NSString*)kIOSurfacePixelFormat: @(kCVPixelFormatType_32BGRA)
        };
        surface = IOSurfaceCreate((__bridge CFDictionaryRef)properties);
        if (!surface) return false;
        glGenTextures(1, &texture);
        glBindTexture(GL_TEXTURE_RECTANGLE, texture);
        CGLError status = CGLTexImageIOSurface2D(context, GL_TEXTURE_RECTANGLE, GL_RGBA8,
            width, height, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, surface, 0);
        glBindTexture(GL_TEXTURE_RECTANGLE, 0);
        if (status == kCGLNoError) {
            glGenFramebuffers(1, &framebuffer);
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_RECTANGLE, texture, 0);
            if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) return true;
        }
        log_error("mac-present: cannot attach %dx%d IOSurface to GL (CGL status %d)", width, height, status);
        release();
        return false;
    }
};

@interface RadiantMacPresentation : NSObject {
@public
    MacPresentBuffer buffers[3];
    unsigned active;
    CALayer* output; // retained by the window's backing layer
}
@end
@implementation RadiantMacPresentation
@end

static char presentation_key;

static bool mac_context_current(NSOpenGLContext* context) {
    // AppKit can restore the native current context after a nested resize
    // callback without updating GLFW's thread-local bookkeeping.
    CGLContextObj actual = context.CGLContextObj;
    return actual && (CGLGetCurrentContext() == actual || CGLSetCurrentContext(actual) == kCGLNoError);
}

static MacPresentBuffer* acquire_buffer(RadiantMacPresentation* presentation,
                                        CGLContextObj context, int width, int height) {
    const unsigned count = sizeof(presentation->buffers) / sizeof(presentation->buffers[0]);
    for (unsigned i = 1; i < count; ++i) {
        MacPresentBuffer& buffer = presentation->buffers[(presentation->active + i) % count];
        if (buffer.surface && !IOSurfaceIsInUse(buffer.surface) &&
            IOSurfaceGetWidth(buffer.surface) == (size_t)width &&
            IOSurfaceGetHeight(buffer.surface) == (size_t)height) return &buffer;
    }
    // an in-flight surface can outlive its GL attachment. Core Animation
    // retains it while the bounded cache moves on to a fresh buffer.
    MacPresentBuffer& next = presentation->buffers[(presentation->active + 1) % count];
    next.release();
    return next.prepare(context, width, height) ? &next : nullptr;
}

static CALayer* attach_output(NSWindow* window, NSOpenGLContext* gl_context) {
    // The layer, rather than the resizable NSGL drawable, owns the displayed image.
    [gl_context clearDrawable];
    NSView* view = window.contentView;
    view.wantsLayer = YES;
    CALayer* output = [CALayer layer];
    output.name = @"Radiant document presentation";
    output.frame = view.layer.bounds;
    output.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
    output.contentsGravity = kCAGravityTopLeft;
    output.masksToBounds = YES;
    output.opaque = YES;
    [view.layer addSublayer:output];
    return output;
}

bool radiant_mac_present(void* cocoa_window, void* ns_gl_context, NativeGlContext* presenter,
                         NativeGlPresentFrame* frame) {
    if (!cocoa_window || !ns_gl_context) return false;
    @autoreleasepool {
        NSWindow* window = (__bridge NSWindow*)cocoa_window;
        NSOpenGLContext* context = (__bridge NSOpenGLContext*)ns_gl_context;
        RadiantMacPresentation* presentation = objc_getAssociatedObject(window, &presentation_key);
        if (!presentation) {
            presentation = [[[RadiantMacPresentation alloc] init] autorelease];
            objc_setAssociatedObject(window, &presentation_key, presentation, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        }

        NativeGlCurrent current(presenter);
        if (!mac_context_current(context)) return false;
        MacPresentBuffer* buffer = acquire_buffer(presentation, context.CGLContextObj,
                                                  frame->framebuffer_width, frame->framebuffer_height);
        if (!buffer) return false;
        frame->top_down_framebuffer = buffer->framebuffer;
        if (!native_gl_present(presenter, frame)) return false;
        // WindowServer can read IOSurface memory as soon as the transaction is sent.
        glFinish();

        [CATransaction begin];
        [CATransaction setDisableActions:YES];
        if (!presentation->output) presentation->output = attach_output(window, context);
        uint32_t rgba = frame->background_rgba;
        CGColorRef background = CGColorCreateSRGB((rgba & 255) / 255.0, ((rgba >> 8) & 255) / 255.0,
                                                   ((rgba >> 16) & 255) / 255.0, 1.0);
        presentation->output.backgroundColor = background;
        if (background) CGColorRelease(background);
        presentation->output.contentsScale = window.backingScaleFactor;
        presentation->output.contents = (__bridge id)buffer->surface;
        presentation->active = (unsigned)(buffer - presentation->buffers); // INT_CAST_OK: cache index
        [CATransaction commit];
        // Live sizing runs in AppKit's tracking loop. Submit the completed layer there.
        [CATransaction flush];
        return true;
    }
}

void radiant_mac_present_destroy(void* cocoa_window, void* ns_gl_context, NativeGlContext* presenter) {
    NSWindow* window = (__bridge NSWindow*)cocoa_window;
    RadiantMacPresentation* presentation = objc_getAssociatedObject(window, &presentation_key);
    if (!presentation) return;
    NativeGlCurrent current(presenter);
    bool context_current = mac_context_current((__bridge NSOpenGLContext*)ns_gl_context);
    [presentation->output removeFromSuperlayer];
    for (MacPresentBuffer& buffer : presentation->buffers) buffer.release(context_current);
    objc_setAssociatedObject(window, &presentation_key, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
}
