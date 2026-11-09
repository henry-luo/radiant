#include "event_sim_script_host.hpp"
#include "lib/provider.hpp"
#include "lib/time_util.h"

static const RadiantEventSimScriptHostOps* event_sim_script_host_ops = nullptr;

bool radiant_event_sim_script_host_register(const RadiantEventSimScriptHostOps* operations) {
    return lambda_provider_register(&event_sim_script_host_ops, operations,
        &RadiantEventSimScriptHostOps::evaluate, &RadiantEventSimScriptHostOps::collect_gc,
        &RadiantEventSimScriptHostOps::animation_frame, &RadiantEventSimScriptHostOps::measure_frames,
        &RadiantEventSimScriptHostOps::clock_ms, &RadiantEventSimScriptHostOps::dialog_response,
        &RadiantEventSimScriptHostOps::selector_matcher, &RadiantEventSimScriptHostOps::element_from_point);
}

bool radiant_event_sim_evaluate(UiContext* ui, const char* source, bool assertion) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    return operations && operations->evaluate(ui, source, assertion);
}

bool radiant_event_sim_collect_gc(UiContext* ui, double count) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    return operations && operations->collect_gc(ui, count);
}

bool radiant_event_sim_animation_frame(UiContext* ui, double time, int* callbacks) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    return operations && operations->animation_frame(ui, time, callbacks);
}

bool radiant_event_sim_measure_frames(UiContext* ui, const char* callback, const char* sample,
                                      size_t frames, double* durations, double* sampling) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    return operations && operations->measure_frames(ui, callback, sample, frames, durations, sampling);
}

double radiant_event_sim_clock_ms(bool relative) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    return operations ? operations->clock_ms(relative) : (double)time_now_ns() / 1e6;
}

void radiant_event_sim_dialog_response(const char* value) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    if (operations) operations->dialog_response(value);
}

bool radiant_event_sim_script_selector(DomDocument* document, Pool* pool, SelectorMatcher** matcher) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    if (!operations) return false;
    *matcher = operations->selector_matcher(document, pool);
    return true;
}

bool radiant_event_sim_script_hit_test(DomDocument* document, float x, float y, DomElement** element) {
    const RadiantEventSimScriptHostOps* operations = lambda_provider_load(&event_sim_script_host_ops);
    if (!operations) return false;
    *element = operations->element_from_point(document, x, y);
    return true;
}
