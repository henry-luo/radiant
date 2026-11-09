#pragma once

#include <stdint.h>

struct UiContext;
struct DomDocument;
struct DomNode;
struct DocState;
struct CssInputCapabilities;
struct DomElement;
struct AnimationTimelineEvent;
struct BehaviorInitPhaseTiming;
union RdtEvent;

// The controller processes the borrowed platform event synchronously.
struct RadiantEventHostOps {
    void (*dispatch)(UiContext* ui, DomDocument* document, RdtEvent* event);
    bool (*focusable)(DomNode* view, bool programmatic);
    void (*sync_pseudo_state)(DomNode* view, uint32_t flag, bool set);
    void (*sync_focus_style)(DomNode* view, uint32_t flags);
    bool (*composition)(UiContext* ui, RdtEvent* event);
    bool (*focused_caret_rect)(UiContext* ui, float* x, float* y, float* width, float* height);
    void (*initialize_behavior)(DomDocument* document, BehaviorInitPhaseTiming* timing);
    void (*autofocus)(UiContext* ui, DomDocument* document);
    void (*window_event)(UiContext* ui, DomDocument* document, const char* type);
    void (*body_load)(UiContext*, DomDocument*);
    bool (*content_visibility)(UiContext*, DomElement*, bool);
    void (*css_event)(UiContext*, DomElement*, const char*, const char*, const char*, double);
    void (*svg_time_event)(UiContext*, DomElement*, const char*, double, double);
    void (*scene_animation)(UiContext*, DomElement*, const char*, uint64_t, const AnimationTimelineEvent&, uint64_t, uint64_t);
    bool (*scene_state)(UiContext*, DomElement*, const char*, const char*, const char*, double);
    bool (*allow_frame_reflow)();
    bool (*frame_tick)(DomDocument*, double);
    bool (*pending_frames)(DomDocument*);
    bool (*focus_element)(DomDocument*, DomNode*);
    bool (*collect_dom)(UiContext*, DomDocument*);
    bool (*editing_animation_active)(DocState*);
    bool (*editing_animation_tick)(UiContext*, double);
    bool (*input_capabilities)(UiContext*, const CssInputCapabilities*);
};

bool radiant_event_host_register(const RadiantEventHostOps* operations);
bool radiant_event_host_dispatch(UiContext* ui, DomDocument* document, RdtEvent* event);
bool radiant_event_host_focusable(DomNode* view, bool programmatic);
void radiant_event_host_sync_pseudo_state(DomNode* view, uint32_t flag, bool set);
void radiant_event_host_sync_focus_style(DomNode* view, uint32_t flags);
bool radiant_event_host_composition(UiContext* ui, RdtEvent* event);
bool radiant_event_host_focused_caret_rect(UiContext* ui, float* x, float* y, float* width, float* height);
void radiant_event_host_initialize_behavior(DomDocument* document, BehaviorInitPhaseTiming* timing = nullptr);
void radiant_event_host_autofocus(UiContext* ui, DomDocument* document);
void radiant_event_host_window_event(UiContext* ui, DomDocument* document, const char* type);
void radiant_event_host_body_load(UiContext* ui, DomDocument* document);
bool radiant_event_host_content_visibility(UiContext* ui, DomElement* element, bool skipped);
void radiant_event_host_css_event(UiContext* ui, DomElement* element, const char* type, const char* name, const char* value, double elapsed);
void radiant_event_host_svg_time_event(UiContext* ui, DomElement* element, const char* type, double detail, double seconds);
void radiant_event_host_scene_animation(UiContext* ui, DomElement* element, const char* clip, uint64_t revision, const AnimationTimelineEvent& event, uint64_t session = 0, uint64_t generation = 0);
bool radiant_event_host_scene_state(UiContext* ui, DomElement* element, const char* controller, const char* from, const char* to, double seconds);
bool radiant_event_host_allow_frame_reflow();
bool radiant_event_host_frame_tick(DomDocument* document, double timestamp_ms);
bool radiant_event_host_pending_frames(DomDocument* document);
bool radiant_event_host_focus_element(DomDocument* document, DomNode* target);
bool radiant_event_host_collect_dom(UiContext* ui, DomDocument* document);
bool radiant_event_host_editing_animation_active(DocState* state);
bool radiant_event_host_editing_animation_tick(UiContext* ui, double timestamp);
bool radiant_event_host_input_capabilities(UiContext* ui, const CssInputCapabilities* capabilities);
