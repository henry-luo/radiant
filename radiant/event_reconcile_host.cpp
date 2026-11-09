#include "event_reconcile_host.hpp"
#include "../lib/provider.hpp"

static const RadiantEventReconcileHostOps* reconcile_host_ops = nullptr;

bool radiant_event_reconcile_host_register(const RadiantEventReconcileHostOps* operations) {
    return lambda_provider_register(&reconcile_host_ops, operations,
        &RadiantEventReconcileHostOps::apply_load_cascade, &RadiantEventReconcileHostOps::prepare_layout,
        &RadiantEventReconcileHostOps::pending_templates, &RadiantEventReconcileHostOps::mutations,
        &RadiantEventReconcileHostOps::attribute, &RadiantEventReconcileHostOps::render_events,
        &RadiantEventReconcileHostOps::follow_rebuild);
}

static bool reconcile_unavailable(const char** reason) {
    if (reason) *reason = "event-controller-unavailable";
    return false;
}

bool radiant_event_reconcile_apply_load(DomDocument* document, const char** reason) {
    const RadiantEventReconcileHostOps* operations = lambda_provider_load(&reconcile_host_ops);
    return operations ? operations->apply_load_cascade(document, reason) : reconcile_unavailable(reason);
}

bool radiant_event_reconcile_prepare_layout(DomDocument* document, const char** reason) {
    const RadiantEventReconcileHostOps* operations = lambda_provider_load(&reconcile_host_ops);
    return operations ? operations->prepare_layout(document, reason) : reconcile_unavailable(reason);
}

bool radiant_event_reconcile_templates(UiContext* ui, DomDocument* document) {
    return lambda_provider_call(lambda_provider_load(&reconcile_host_ops), &RadiantEventReconcileHostOps::pending_templates, ui, document);
}

void radiant_event_reconcile_mutations(UiContext* ui, DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&reconcile_host_ops), &RadiantEventReconcileHostOps::mutations, ui, document);
}

void radiant_event_reconcile_attribute(DomElement* element, const char* name, const char* old_value) {
    lambda_provider_call(lambda_provider_load(&reconcile_host_ops), &RadiantEventReconcileHostOps::attribute, element, name, old_value);
}

void radiant_event_reconcile_render_events(DomDocument* document, DomNode* root) {
    lambda_provider_call(lambda_provider_load(&reconcile_host_ops), &RadiantEventReconcileHostOps::render_events, document, root);
}

void radiant_event_reconcile_follow_rebuild(DomDocument* document, DomNode* old_root, DomNode* new_root) {
    lambda_provider_call(lambda_provider_load(&reconcile_host_ops), &RadiantEventReconcileHostOps::follow_rebuild, document, old_root, new_root);
}
