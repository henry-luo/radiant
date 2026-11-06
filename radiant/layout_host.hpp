#pragma once

struct DomDocument;
struct DomElement;
struct UiContext;

// A script host owns execution and observer state around native layout.
struct RadiantLayoutHostOps {
    bool (*suspend_watchdog)();
    void (*resume_watchdog)();
    void (*post_layout)();
    void* (*capture_iframe_context)(DomDocument* parent);
    void (*restore_iframe_context)(UiContext* ui, DomDocument* parent, void* context);
    bool (*defer_iframe_load)();
    void (*iframe_load_complete)(DomDocument* parent, DomElement* frame);
    float (*geometry_fallback_dimension)(DomElement* element, bool width_axis);
};

bool radiant_layout_host_register(const RadiantLayoutHostOps* operations);
bool radiant_layout_suspend_watchdog();
void radiant_layout_resume_watchdog();
void radiant_layout_post_observers();
void* radiant_layout_capture_iframe_context(DomDocument* parent);
void radiant_layout_restore_iframe_context(UiContext* ui, DomDocument* parent, void* context);
bool radiant_layout_defer_iframe_load();
void radiant_layout_iframe_load_complete(DomDocument* parent, DomElement* frame);

float radiant_layout_geometry_fallback_dimension(DomElement* element, bool width_axis);
