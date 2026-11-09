#pragma once

#include "scene3d_audio.hpp"
#include "scene3d_source.hpp"
#include "../audio.hpp"
#include "../audio_cues.hpp"
#include "../audio_cue_owner.hpp"
#include "../audio_render.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"

static constexpr unsigned AUDIO_SCENE_SOURCES = 32, AUDIO_SCENE_CLIPS = 16;
static constexpr float AUDIO_COORDINATE_LIMIT = 1e6f;
enum AudioSpatial { AUDIO_STEREO, AUDIO_2D, AUDIO_3D };
enum AudioDistance { AUDIO_INVERSE, AUDIO_LINEAR, AUDIO_NO_DISTANCE };
enum AudioNumber { AUDIO_GAIN, AUDIO_PITCH, AUDIO_SMOOTH, AUDIO_LOWPASS, AUDIO_REFERENCE, AUDIO_MAXIMUM,
                   AUDIO_ROLLOFF, AUDIO_INNER, AUDIO_OUTER, AUDIO_CONE_GAIN, AUDIO_NUMBERS };
struct AudioMotion {
    Scene3dVec position, velocity;
    double time;
    bool valid, reset;
};
struct AudioDescription {
    char id[128], clip[128], bus[128];
    float values[AUDIO_NUMBERS];
    Scene3dVec velocity;
    AudioSpatial spatial;
    AudioDistance distance;
    bool loop, doppler, explicit_velocity, pitch_follow;
};
struct AudioSource {
    DomNodeRef node;
    AudioDescription description;
    AudioMotion motion;
    RdtAudioParameters parameters;
    uint64_t identity, player;
    AudioRouteId route;
    float unscaled_pitch;
};
struct AudioListener {
    DomNodeRef node;
    char id[128];
    AudioMotion motion;
    Scene3dVec velocity, right, forward;
    bool explicit_velocity, camera;
};
struct AudioClip {
    char id[128];
    uint64_t identity, token;
    size_t frames;
};
struct Scene3dAudio {
    DomDocument* document;
    DomNodeRef root;
    Scene3dRuntime* runtime;
    Scene3dAnimationState* animation;
    Scene3dPhysics* physics;
    AudioSource sources[AUDIO_SCENE_SOURCES];
    AudioClip clips[AUDIO_SCENE_CLIPS];
    AudioListener listener;
    unsigned count, clip_count;
    uint64_t epoch, generation, sample_sequence, identity;
    float speed;
    AudioGraphId graph;
    AudioSubmixId submix;
    uint64_t routing_generation;
    AudioCueQueue* cues;
    AudioCueOwner* cue_owner;
    uint64_t cue_epoch;
    char diagnostic[512];
};
struct AudioGather {
    DomElement* root;
    DomElement* sources[AUDIO_SCENE_SOURCES];
    DomElement* explicit_listener = nullptr;
    DomElement* camera = nullptr;
    DomElement* selected = nullptr;
    unsigned count = 0;
    const char* listener_id;
    const char* camera_id;
    DomElement* buses[AUDIO_GRAPH_BUSES] = {};
    unsigned bus_count = 0;
};
bool scene3d_audio_description(DomElement* node, AudioDescription* description);
Scene3dVisit scene3d_audio_gather(DomElement* node, void* context);
const char* scene3d_audio_bus_config(const AudioGather& gather, AudioGraphConfig* result);
bool scene3d_audio_listener_frame(Scene3dAudio* state, DomElement* root, AudioListener* listener, double time);
bool scene3d_audio_source_frame(Scene3dAudio* state, DomElement* root, AudioSource* source,
                                 const AudioListener& listener, float speed, double time);
void scene3d_audio_cues_collect(Scene3dAudio* state);
const char* scene3d_audio_cues_sync(Scene3dAudio* state);
const char* scene3d_audio_listener_description(Scene3dAudio* state, const AudioGather& gather, AudioListener* result);
struct AudioEffectField { const char* key; size_t offset; };
const AudioEffectField* scene3d_audio_effect_fields(size_t* count);
