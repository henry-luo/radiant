#include "scene3d_navigation_event_host.hpp"
#include "../../lib/provider.hpp"

static const Scene3dNavigationEventHostOps* navigation_event_host_ops = nullptr;

bool scene3d_navigation_event_host_register(const Scene3dNavigationEventHostOps* operations) {
    return lambda_provider_register(&navigation_event_host_ops, operations, &Scene3dNavigationEventHostOps::publish);
}

bool scene3d_navigation_event_publish(DomElement* target, Scene3dNavigation* state,
                                      Scene3dNavigationAgent* agent, const char* type) {
    const Scene3dNavigationEventHostOps* operations = lambda_provider_load(&navigation_event_host_ops);
    return !operations || operations->publish(target, state, agent, type);
}
