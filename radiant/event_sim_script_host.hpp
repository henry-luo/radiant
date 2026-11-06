#pragma once

#include <stddef.h>
struct UiContext;
struct DomDocument;
struct DomElement;
struct Pool;
struct SelectorMatcher;

// Script-only test commands are optional; native capture remains independent.
struct RadiantEventSimScriptHostOps {
    bool (*evaluate)(UiContext* ui, const char* source, bool assertion);
    bool (*collect_gc)(UiContext* ui, double count);
    bool (*animation_frame)(UiContext* ui, double time, int* callbacks);
    bool (*measure_frames)(UiContext* ui, const char* callback, const char* sample,
                           size_t frames, double* durations, double* sampling);
    double (*clock_ms)(bool relative);
    void (*dialog_response)(const char* value);
    SelectorMatcher* (*selector_matcher)(DomDocument* document, Pool* pool);
    DomElement* (*element_from_point)(DomDocument* document, float x, float y);
};

bool radiant_event_sim_script_host_register(const RadiantEventSimScriptHostOps* operations);
bool radiant_event_sim_evaluate(UiContext* ui, const char* source, bool assertion);
bool radiant_event_sim_collect_gc(UiContext* ui, double count);
bool radiant_event_sim_animation_frame(UiContext* ui, double time, int* callbacks);
bool radiant_event_sim_measure_frames(UiContext* ui, const char* callback, const char* sample,
                                      size_t frames, double* durations, double* sampling);

// Native-only event simulation uses the platform monotonic clock.
double radiant_event_sim_clock_ms(bool relative);
void radiant_event_sim_dialog_response(const char* value);
// The return value indicates whether a host was present; a null matcher can
// still report allocation failure from a registered host.
bool radiant_event_sim_script_selector(DomDocument* document, Pool* pool, SelectorMatcher** matcher);
bool radiant_event_sim_script_hit_test(DomDocument* document, float x, float y, DomElement** element);
