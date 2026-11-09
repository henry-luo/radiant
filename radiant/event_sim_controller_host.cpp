#include "event_sim_controller_host.hpp"
#include "lib/provider.hpp"

static const RadiantEventSimControllerHostOps* event_sim_controller_ops = nullptr;

bool radiant_event_sim_controller_host_register(const RadiantEventSimControllerHostOps* operations) {
    return lambda_provider_register(&event_sim_controller_ops, operations,
        &RadiantEventSimControllerHostOps::pointer, &RadiantEventSimControllerHostOps::touch,
        &RadiantEventSimControllerHostOps::select_change, &RadiantEventSimControllerHostOps::text_drag_drop);
}

bool radiant_event_sim_controller_pointer(UiContext* ui, DomNode* target, const char* type, double x, double y, int button, int buttons, int mods, const char* pointer_type) {
    return lambda_provider_call(lambda_provider_load(&event_sim_controller_ops), &RadiantEventSimControllerHostOps::pointer, ui, target, type, x, y, button, buttons, mods, pointer_type);
}

bool radiant_event_sim_controller_touch(UiContext* ui, DomNode* target, const char* type, double x, double y, int mods, bool active, double timestamp) {
    return lambda_provider_call(lambda_provider_load(&event_sim_controller_ops), &RadiantEventSimControllerHostOps::touch, ui, target, type, x, y, mods, active, timestamp);
}

bool radiant_event_sim_controller_select_change(UiContext* ui, DomNode* target, int index) {
    return lambda_provider_call(lambda_provider_load(&event_sim_controller_ops), &RadiantEventSimControllerHostOps::select_change, ui, target, index);
}

bool radiant_event_sim_controller_text_drag_drop(UiContext* ui, DomNode* source, uint32_t source_start, uint32_t source_end, DomNode* target, uint32_t target_start, uint32_t target_end, const char* payload, const char* html, bool move) {
    return lambda_provider_call(lambda_provider_load(&event_sim_controller_ops), &RadiantEventSimControllerHostOps::text_drag_drop, ui, source, source_start, source_end, target, target_start, target_end, payload, html, move);
}
