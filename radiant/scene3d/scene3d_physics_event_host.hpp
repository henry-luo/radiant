#pragma once

struct DomElement;
struct PhysicsScene;
struct PhysicsEvent;

// The solver retains its events; the host copies these borrowed values synchronously.
struct Scene3dPhysicsEventHostOps {
    bool (*publish)(DomElement* target, const PhysicsScene* scene,
                    const PhysicsEvent* event, const char* type);
};
bool scene3d_physics_event_host_register(const Scene3dPhysicsEventHostOps* operations);
bool scene3d_physics_event_publish(DomElement* target, const PhysicsScene* scene,
                                   const PhysicsEvent* event, const char* type);
