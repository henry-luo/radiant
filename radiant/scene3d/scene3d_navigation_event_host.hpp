#pragma once

struct DomElement;
struct Scene3dNavigation;
struct Scene3dNavigationAgent;

// The host copies borrowed navigation state synchronously before queuing delivery.
struct Scene3dNavigationEventHostOps {
    bool (*publish)(DomElement* target, Scene3dNavigation* state,
                    Scene3dNavigationAgent* agent, const char* type);
};
bool scene3d_navigation_event_host_register(const Scene3dNavigationEventHostOps* operations);
bool scene3d_navigation_event_publish(DomElement* target, Scene3dNavigation* state,
                                      Scene3dNavigationAgent* agent, const char* type);
