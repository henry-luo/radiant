// PE6 (Radiant_Design_Paint_Engine.md): macOS pen capture beside GLFW.
//
// GLFW reports no tablet axes. Its cursor and button callbacks call radiant_pen_fill, which reads
// the pen fields of the AppKit event being dispatched (NSApp.currentEvent). A local event monitor
// tracks tablet proximity, the only source of the device kind, and turns tablet-point events that
// carry no mouse motion (a pressure or tilt change while the pen rests), which GLFW ignores, into
// moves at the pen's position. While a pen is in proximity, AppKit's mouse coalescing is off, so
// every tablet sample reaches the input batch, which keeps them for the dispatched move.
//
// Like ime_mac.mm, this file avoids view.hpp, whose Rect conflicts with AppKit's MacTypes, and
// imports AppKit before Lambda's headers, which define `null`.

#ifdef __APPLE__
#import <Cocoa/Cocoa.h>
#endif

#define RADIANT_EVENT_CORE_ONLY
#include "pen_mac.hpp"
#undef RADIANT_EVENT_CORE_ONLY

#ifdef __APPLE__

#include <math.h>

#include "platform.hpp"
#include "lib/log.h"

namespace {

id g_pen_monitor = nil;                          // retained; the build is not ARC
RadiantWindow* g_pen_window = nullptr;
RdtPointerType g_pen_kind = RDT_POINTER_MOUSE;   // the tablet device in proximity
bool g_pen_coalescing = true;                    // AppKit's setting before a pen came near

const uint32_t PEN_POINTER_ID = 1;               // one pen at a time on a desktop tablet

bool pen_mouse_event(NSEvent* e) {
    switch (e.type) {
        case NSEventTypeMouseMoved:
        case NSEventTypeLeftMouseDown: case NSEventTypeLeftMouseUp: case NSEventTypeLeftMouseDragged:
        case NSEventTypeRightMouseDown: case NSEventTypeRightMouseUp: case NSEventTypeRightMouseDragged:
        case NSEventTypeOtherMouseDown: case NSEventTypeOtherMouseUp: case NSEventTypeOtherMouseDragged:
            return true;
        default:
            return false;
    }
}

// AppKit reads the subtype, tilt and proximity fields only on the event kinds that carry them.
bool pen_tablet_point(NSEvent* e) {
    if (e.type == NSEventTypeTabletPoint) return true;
    return pen_mouse_event(e) && e.subtype == NSEventSubtypeTabletPoint;
}

bool pen_tablet_proximity(NSEvent* e) {
    if (e.type == NSEventTypeTabletProximity) return true;
    return pen_mouse_event(e) && e.subtype == NSEventSubtypeTabletProximity;
}

void pen_set_coalescing(bool pen_near) {
    if (pen_near && g_pen_kind == RDT_POINTER_MOUSE) g_pen_coalescing = [NSEvent isMouseCoalescingEnabled];
    [NSEvent setMouseCoalescingEnabled:pen_near ? NO : g_pen_coalescing];
}

void pen_note_proximity(NSEvent* e) {
    RdtPointerType kind = RDT_POINTER_MOUSE;
    if (e.isEnteringProximity) {
        // a puck (cursor device) moves the pointer as a mouse does
        if (e.pointingDeviceType == NSPointingDeviceTypePen) kind = RDT_POINTER_PEN;
        else if (e.pointingDeviceType == NSPointingDeviceTypeEraser) kind = RDT_POINTER_ERASER;
    }
    pen_set_coalescing(kind != RDT_POINTER_MOUSE);
    g_pen_kind = kind;
    log_info("pen-mac: proximity %s, device %d", e.isEnteringProximity ? "entered" : "left", (int)kind);
}

float pen_clamp(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

void pen_fill_axes(MousePositionEvent* event, NSEvent* e) {
    // a pen already near when capture started sent no proximity event
    event->pointer_type = g_pen_kind != RDT_POINTER_MOUSE ? g_pen_kind : RDT_POINTER_PEN;
    event->pointer_id = PEN_POINTER_ID;
    float pressure = 0.0f, tangential = 0.0f, rotation = 0.0f;
    NSPoint tilt = NSZeroPoint;
    // a driver that omits an axis makes AppKit raise on its accessor; that axis reads as zero.
    @try { pressure = e.pressure; tilt = e.tilt; } @catch (NSException*) {}
    @try { tangential = e.tangentialPressure; } @catch (NSException*) {}
    @try { rotation = e.rotation; } @catch (NSException*) {}
    event->pressure = pen_clamp(pressure, 0.0f, 1.0f);
    event->tangential_pressure = pen_clamp(tangential, -1.0f, 1.0f);
    // AppKit's tilt is -1..1 with y positive away from the user; W3C's tiltY is positive toward them.
    event->tilt_x = pen_clamp((float)tilt.x * 90.0f, -90.0f, 90.0f);
    event->tilt_y = pen_clamp((float)-tilt.y * 90.0f, -90.0f, 90.0f);
    // AppKit turns counterclockwise; W3C's twist is clockwise in 0..359.
    float twist = fmodf(360.0f - rotation, 360.0f);
    event->twist = twist < 0.0f ? twist + 360.0f : twist;
    // the sample's own time, moved onto the backend clock, keeps spacing exact when AppKit
    // delivers queued samples together.
    double age = [NSProcessInfo processInfo].systemUptime - e.timestamp;
    if (age >= 0.0 && age < 1.0) event->timestamp = radiant_platform_time() - age;
}

// a tablet point without mouse motion: a move at the pen's position in the window.
void pen_submit_point(NSEvent* e) {
    if (!g_pen_window || e.window != (__bridge NSWindow*)radiant_platform_native_window(g_pen_window)) return;
    NSView* content = e.window.contentView;
    if (!content) return;
    NSPoint p = [content convertPoint:e.locationInWindow fromView:nil];
    // GLFW's content view is unflipped and reports logical coordinates from the top-left
    float y = content.isFlipped ? (float)p.y : (float)(content.bounds.size.height - p.y);
    RdtEvent event;
    rdt_event_set_mouse_position(&event, RDT_EVENT_MOUSE_MOVE, (float)p.x, y, radiant_platform_time());
    pen_fill_axes(&event.mouse_position, e);
    radiant_host_input(g_pen_window, &event);
}

}  // namespace

void radiant_pen_enable(RadiantWindow* window, bool enabled) {
    if (enabled) {
        g_pen_window = window;
        if (g_pen_monitor) return;
        NSEventMask mask = NSEventMaskTabletPoint | NSEventMaskTabletProximity | NSEventMaskMouseMoved |
            NSEventMaskLeftMouseDragged | NSEventMaskRightMouseDragged | NSEventMaskOtherMouseDragged |
            NSEventMaskLeftMouseDown | NSEventMaskLeftMouseUp;
        g_pen_monitor = [[NSEvent addLocalMonitorForEventsMatchingMask:mask handler:^NSEvent*(NSEvent* e) {
            if (pen_tablet_proximity(e)) pen_note_proximity(e);
            else if (e.type == NSEventTypeTabletPoint) pen_submit_point(e);
            return e;  // GLFW still receives every event
        }] retain];
        log_info("pen-mac: capture started");
        return;
    }
    if (window != g_pen_window) return;
    if (g_pen_monitor) {
        [NSEvent removeMonitor:g_pen_monitor];
        [g_pen_monitor release];
        g_pen_monitor = nil;
    }
    if (g_pen_kind != RDT_POINTER_MOUSE) pen_set_coalescing(false);
    g_pen_kind = RDT_POINTER_MOUSE;
    g_pen_window = nullptr;
    log_info("pen-mac: capture stopped");
}

void radiant_pen_fill(MousePositionEvent* event) {
    if (!event) return;
    NSEvent* e = [NSApp currentEvent];
    if (e && pen_tablet_point(e)) pen_fill_axes(event, e);
}

#else  // !__APPLE__

// no native pen capture yet (PE-O4): pointers stay mice.
void radiant_pen_enable(RadiantWindow* window, bool enabled) { (void)window; (void)enabled; }
void radiant_pen_fill(MousePositionEvent* event) { (void)event; }

#endif  // __APPLE__
