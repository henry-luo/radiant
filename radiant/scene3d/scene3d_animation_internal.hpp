#pragma once

#include "scene3d_animation.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"

struct Scene3dAnimationBinding {
    Scene3dAnimationBinding* next;
    DomNodeRef node;
    uint64_t identity;
    char property[32];
    AnimationValue value;
    bool root_motion;
};
struct Scene3dAnimationClip {
    Scene3dAnimationClip* next;
    char name[128];
    AnimationActionState* action;
    bool autoplay, controlled;
};
struct Scene3dAnimationNumbers;
struct Scene3dMotionController;
static constexpr unsigned SCENE_MOTION_CONTROLLER_LIMIT = 16;

struct Scene3dAnimationState {
    DomDocument* document;
    DomNodeRef root;
    Pool* pool;
    AnimationMixerState* mixer;
    Scene3dAnimationBinding* bindings;
    Scene3dAnimationClip* clips;
    Scene3dAnimationNumbers* numbers;
    Scene3dMotionController* controllers;
    DomNodeRef pending[SCENE_MOTION_CONTROLLER_LIMIT];
    unsigned pending_count;
    Scene3dRuntime* runtime;
    uint64_t epoch, generation, revision, command_revision, event_replays, source_hash, number_count, text_bytes;
    uint32_t source_generation;
    unsigned binding_count, clip_count;
    bool automatic, quiet, coupled;
    const char* diagnostic;
    Scene3dAnimationEvents* replay_events;
};

struct Scene3dAnimationCommands {
    uint64_t revision, world_identity;
    unsigned count;
    AnimationCommand values[ANIMATION_COMMAND_LIMIT];
};

Scene3dAnimationClip* scene3d_animation_clip(Scene3dAnimationState* state, const char* name);
uint64_t scene3d_animation_world(Scene3dAnimationState* state);
