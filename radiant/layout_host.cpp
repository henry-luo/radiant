#include "layout_host.hpp"
#include "lib/provider.hpp"

static const RadiantLayoutHostOps* layout_host_ops = nullptr;

bool radiant_layout_host_register(const RadiantLayoutHostOps* operations) {
    return lambda_provider_register(&layout_host_ops, operations,
        &RadiantLayoutHostOps::suspend_watchdog, &RadiantLayoutHostOps::resume_watchdog,
        &RadiantLayoutHostOps::post_layout, &RadiantLayoutHostOps::capture_iframe_context,
        &RadiantLayoutHostOps::restore_iframe_context, &RadiantLayoutHostOps::defer_iframe_load,
        &RadiantLayoutHostOps::iframe_load_complete, &RadiantLayoutHostOps::geometry_fallback_dimension);
}

bool radiant_layout_suspend_watchdog() {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    return operations && operations->suspend_watchdog();
}

void radiant_layout_resume_watchdog() {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    if (operations) operations->resume_watchdog();
}

void radiant_layout_post_observers() {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    if (operations) operations->post_layout();
}

void* radiant_layout_capture_iframe_context(DomDocument* parent) {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    return operations ? operations->capture_iframe_context(parent) : nullptr;
}

void radiant_layout_restore_iframe_context(UiContext* ui, DomDocument* parent, void* context) {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    if (operations) operations->restore_iframe_context(ui, parent, context);
}

bool radiant_layout_defer_iframe_load() {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    return operations && operations->defer_iframe_load();
}

void radiant_layout_iframe_load_complete(DomDocument* parent, DomElement* frame) {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    if (operations) operations->iframe_load_complete(parent, frame);
}

float radiant_layout_geometry_fallback_dimension(DomElement* element, bool width_axis) {
    const RadiantLayoutHostOps* operations = lambda_provider_load(&layout_host_ops);
    return operations ? operations->geometry_fallback_dimension(element, width_axis) : 0.0f;
}
