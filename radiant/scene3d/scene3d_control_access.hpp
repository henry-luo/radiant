#pragma once

struct DomElement;
struct Scene3dEntry;
struct Scene3dRuntime;
struct Scene3dPhysics;
struct Scene3dAnimationState;
struct Scene3dAudio;
struct Scene3dParticles;
struct Scene3dNavigation;

// Borrowed services are valid while their document-owned scene entry is live.
struct Scene3dControlServices {
    Scene3dRuntime* runtime;
    Scene3dPhysics* physics;
    Scene3dAnimationState* animation;
    Scene3dAudio* audio;
    Scene3dParticles* particles;
};

Scene3dEntry* scene3d_control_open(DomElement* root);
bool scene3d_control_prepare_services(Scene3dEntry* entry, DomElement* root);
bool scene3d_control_prepare_audio(Scene3dEntry* entry, DomElement* root);
Scene3dNavigation* scene3d_control_navigation(Scene3dEntry* entry, DomElement* root);
Scene3dControlServices scene3d_control_services(Scene3dEntry* entry);
