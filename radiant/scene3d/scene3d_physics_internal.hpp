#pragma once

#include "scene3d_physics.hpp"
#include "scene3d_animation.hpp"
#include "lambda/core/physics_scene.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"

// Native physics ownership shared with the Lambda value adapter.
struct PhysicsRagdollPlayback {
    PhysicsRagdollMode mode;
    double elapsed;
    bool initialized;
    PhysicsPose starts[64];
};
static constexpr size_t PHYSICS_ROOT_LIMIT = 32;
struct PhysicsRootMotion {
    DomNodeRef body, source;
    char source_id[128];
    PhysicsCharacterMove movement;
    PhysicsCharacterState feedback;
    double consumed[3], precision;
    PhysicsQuat consumed_rotation, requested_rotation, achieved_rotation;
    bool has_translation, has_rotation;
    uint64_t epoch, sequence, world_identity, tick;
    bool initialized, blocked;
};
struct PhysicsRootStep {
    PhysicsCharacterState movement;
    Scene3dRootMotion source;
    PhysicsQuat requested_rotation, achieved_rotation;
    bool blocked;
};
struct Scene3dPhysics {
    DomDocument* document = nullptr;
    DomNodeRef root = {};
    PhysicsScene scene;
    PhysicsWorld* world = nullptr;
    PhysicsState published = {};
    Scene3dRuntime* runtime = nullptr;
    uint64_t epoch = 0, generation = 1;
    uint32_t source_generation = 0;
    uint64_t world_identity = 0;
    uint64_t* next_identity = nullptr;
    bool automatic = false, playing = false;
    lam::ArrayList<PhysicsRagdollPlayback> ragdolls;
    lam::ArrayList<PhysicsRootMotion> roots{MEM_CAT_RENDER, 0};
    lam::ArrayList<PhysicsRootMotion> pending_roots{MEM_CAT_RENDER, 0};
    bool root_definitions_pending = false;
    lam::ArrayList<PhysicsRootStep> root_steps{MEM_CAT_RENDER, 0};
    bool root_committing = false;
    lam::ArrayList<PhysicsEvent> event_buffer;
    bool collecting_events = false;
    bool advancing = false;
    bool quiet = false;
    Scene3dAnimationState* replay_animation = nullptr;
};
bool scene3d_physics_scene_identity_available(Scene3dPhysics* state, PhysicsError* error);
void scene3d_physics_scene_identity_commit(Scene3dPhysics* state);
