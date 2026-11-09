#include "event_host.hpp"
#include "../lib/provider.hpp"

static const RadiantEventHostOps* event_host_ops = nullptr;

bool radiant_event_host_register(const RadiantEventHostOps* operations) {
    return lambda_provider_register(&event_host_ops, operations, &RadiantEventHostOps::dispatch, &RadiantEventHostOps::focusable,
        &RadiantEventHostOps::sync_pseudo_state, &RadiantEventHostOps::sync_focus_style,
        &RadiantEventHostOps::composition, &RadiantEventHostOps::focused_caret_rect,
        &RadiantEventHostOps::initialize_behavior, &RadiantEventHostOps::autofocus,
        &RadiantEventHostOps::window_event, &RadiantEventHostOps::body_load,
        &RadiantEventHostOps::content_visibility, &RadiantEventHostOps::css_event,
        &RadiantEventHostOps::svg_time_event, &RadiantEventHostOps::scene_animation,
        &RadiantEventHostOps::scene_state,
        &RadiantEventHostOps::allow_frame_reflow, &RadiantEventHostOps::frame_tick,
        &RadiantEventHostOps::pending_frames, &RadiantEventHostOps::focus_element,
        &RadiantEventHostOps::collect_dom, &RadiantEventHostOps::editing_animation_active,
        &RadiantEventHostOps::editing_animation_tick, &RadiantEventHostOps::input_capabilities);
}

bool radiant_event_host_dispatch(UiContext* ui, DomDocument* document, RdtEvent* event) {
    const RadiantEventHostOps* operations = lambda_provider_load(&event_host_ops);
    if (!operations || !event) return false;
    operations->dispatch(ui, document, event);
    return true;
}

bool radiant_event_host_focusable(DomNode* view, bool programmatic) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops),
                                &RadiantEventHostOps::focusable, view, programmatic);
}

void radiant_event_host_sync_pseudo_state(DomNode* view, uint32_t flag, bool set) {
    lambda_provider_call(lambda_provider_load(&event_host_ops),
                         &RadiantEventHostOps::sync_pseudo_state, view, flag, set);
}

void radiant_event_host_sync_focus_style(DomNode* view, uint32_t flags) {
    lambda_provider_call(lambda_provider_load(&event_host_ops),
                         &RadiantEventHostOps::sync_focus_style, view, flags);
}

bool radiant_event_host_composition(UiContext* ui, RdtEvent* event) {
    return event && lambda_provider_call(lambda_provider_load(&event_host_ops),
                                         &RadiantEventHostOps::composition, ui, event);
}

bool radiant_event_host_focused_caret_rect(UiContext* ui, float* x, float* y, float* width, float* height) {
    const RadiantEventHostOps* operations = lambda_provider_load(&event_host_ops);
    if (operations) return operations->focused_caret_rect(ui, x, y, width, height);
    if (x) *x = 0.0f;
    if (y) *y = 0.0f;
    if (width) *width = 0.0f;
    if (height) *height = 0.0f;
    return false;
}

void radiant_event_host_initialize_behavior(DomDocument* document, BehaviorInitPhaseTiming* timing) {
    lambda_provider_call(lambda_provider_load(&event_host_ops),
                         &RadiantEventHostOps::initialize_behavior, document, timing);
}

void radiant_event_host_autofocus(UiContext* ui, DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&event_host_ops),
                         &RadiantEventHostOps::autofocus, ui, document);
}

void radiant_event_host_window_event(UiContext* ui, DomDocument* document, const char* type) {
    lambda_provider_call(lambda_provider_load(&event_host_ops),
                         &RadiantEventHostOps::window_event, ui, document, type);
}

void radiant_event_host_body_load(UiContext* ui, DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::body_load, ui, document);
}

bool radiant_event_host_content_visibility(UiContext* ui, DomElement* element, bool skipped) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::content_visibility, ui, element, skipped);
}

void radiant_event_host_css_event(UiContext* ui, DomElement* element, const char* type, const char* name, const char* value, double elapsed) {
    lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::css_event, ui, element, type, name, value, elapsed);
}

void radiant_event_host_svg_time_event(UiContext* ui, DomElement* element, const char* type, double detail, double seconds) {
    lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::svg_time_event, ui, element, type, detail, seconds);
}

void radiant_event_host_scene_animation(UiContext* ui, DomElement* element, const char* clip, uint64_t revision, const AnimationTimelineEvent& event, uint64_t session, uint64_t generation) {
    // The timeline record is borrowed only for the synchronous host call.
    const RadiantEventHostOps* operations = lambda_provider_load(&event_host_ops);
    if (operations) operations->scene_animation(ui, element, clip, revision, event, session, generation);
}

bool radiant_event_host_scene_state(UiContext* ui, DomElement* element, const char* controller, const char* from, const char* to, double seconds) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::scene_state, ui, element, controller, from, to, seconds);
}

bool radiant_event_host_allow_frame_reflow() {
    const RadiantEventHostOps* operations = lambda_provider_load(&event_host_ops);
    return !operations || operations->allow_frame_reflow();
}

bool radiant_event_host_frame_tick(DomDocument* document, double timestamp_ms) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::frame_tick, document, timestamp_ms);
}

bool radiant_event_host_pending_frames(DomDocument* document) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::pending_frames, document);
}

bool radiant_event_host_focus_element(DomDocument* document, DomNode* target) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::focus_element, document, target);
}

bool radiant_event_host_collect_dom(UiContext* ui, DomDocument* document) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::collect_dom, ui, document);
}

bool radiant_event_host_editing_animation_active(DocState* state) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::editing_animation_active, state);
}

bool radiant_event_host_editing_animation_tick(UiContext* ui, double timestamp) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::editing_animation_tick, ui, timestamp);
}

bool radiant_event_host_input_capabilities(UiContext* ui, const CssInputCapabilities* capabilities) {
    return lambda_provider_call(lambda_provider_load(&event_host_ops), &RadiantEventHostOps::input_capabilities, ui, capabilities);
}
