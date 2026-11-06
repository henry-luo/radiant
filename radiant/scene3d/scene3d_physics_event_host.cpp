#include "scene3d_physics_event_host.hpp"
#include "../../lib/provider.hpp"

static const Scene3dPhysicsEventHostOps* physics_event_host_ops = nullptr;

bool scene3d_physics_event_host_register(const Scene3dPhysicsEventHostOps* operations) {
    return lambda_provider_register(&physics_event_host_ops, operations, &Scene3dPhysicsEventHostOps::publish);
}

bool scene3d_physics_event_publish(DomElement* target, const PhysicsScene* scene,
                                   const PhysicsEvent* event, const char* type) {
    const Scene3dPhysicsEventHostOps* operations = lambda_provider_load(&physics_event_host_ops);
    // Native simulations retain their event records even when no script host is present.
    return !operations || operations->publish(target, scene, event, type);
}
