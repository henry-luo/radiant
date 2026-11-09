#include "event_navigation_host.hpp"
#include "lib/provider.hpp"

static const RadiantEventNavigationHostOps* navigation_host_ops = nullptr;

bool radiant_event_navigation_host_register(const RadiantEventNavigationHostOps* operations) {
    return lambda_provider_register(&navigation_host_ops, operations,
        &RadiantEventNavigationHostOps::commit_navigation, &RadiantEventNavigationHostOps::poll_loads,
        &RadiantEventNavigationHostOps::drain_loads, &RadiantEventNavigationHostOps::pending_loads,
        &RadiantEventNavigationHostOps::commit_loads, &RadiantEventNavigationHostOps::cancel_loads,
        &RadiantEventNavigationHostOps::navigate, &RadiantEventNavigationHostOps::schedule_iframe);
}

bool radiant_event_navigation_commit(UiContext* ui) {
    return lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::commit_navigation, ui);
}

bool radiant_event_navigation_poll_loads(UiContext* ui) {
    const RadiantEventNavigationHostOps* operations = lambda_provider_load(&navigation_host_ops);
    // A native host with no controller has no outstanding script-owned loads.
    return !operations || operations->poll_loads(ui);
}

void radiant_event_navigation_drain_loads(UiContext* ui) {
    lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::drain_loads, ui);
}

bool radiant_event_navigation_pending_loads(UiContext* ui) {
    return lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::pending_loads, ui);
}

bool radiant_event_navigation_commit_loads(UiContext* ui) {
    return lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::commit_loads, ui);
}

void radiant_event_navigation_cancel_loads(DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::cancel_loads, document);
}

bool radiant_event_navigation_location(UiContext* ui, DomDocument* document, const char* url) {
    return lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::navigate, ui, document, url);
}

bool radiant_event_navigation_schedule_iframe(UiContext* ui, DomElement* iframe, const char* url, int width, int height) {
    return lambda_provider_call(lambda_provider_load(&navigation_host_ops), &RadiantEventNavigationHostOps::schedule_iframe, ui, iframe, url, width, height);
}
