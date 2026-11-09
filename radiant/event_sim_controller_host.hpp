#pragma once

#include <stdint.h>
struct UiContext;
struct DomNode;

// The native simulator asks the controller to deliver scripted input.
struct RadiantEventSimControllerHostOps {
    bool (*pointer)(UiContext*, DomNode*, const char*, double, double, int, int, int, const char*);
    bool (*touch)(UiContext*, DomNode*, const char*, double, double, int, bool, double);
    bool (*select_change)(UiContext*, DomNode*, int);
    bool (*text_drag_drop)(UiContext*, DomNode*, uint32_t, uint32_t, DomNode*, uint32_t, uint32_t, const char*, const char*, bool);
};

bool radiant_event_sim_controller_host_register(const RadiantEventSimControllerHostOps* operations);
bool radiant_event_sim_controller_pointer(UiContext* ui, DomNode* target, const char* type, double x, double y, int button, int buttons, int mods, const char* pointer_type);
bool radiant_event_sim_controller_touch(UiContext* ui, DomNode* target, const char* type, double x, double y, int mods, bool active, double timestamp);
bool radiant_event_sim_controller_select_change(UiContext* ui, DomNode* target, int index);
bool radiant_event_sim_controller_text_drag_drop(UiContext* ui, DomNode* source, uint32_t source_start, uint32_t source_end, DomNode* target, uint32_t target_start, uint32_t target_end, const char* payload, const char* html, bool move);
