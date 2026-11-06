#pragma once

#include "scene3d.hpp"
#include "scene3d_source.hpp"
#include "scene3d_animation.hpp"
#include "../../lambda/input/css/dom_lifecycle.hpp"

// Native simulation state shared with the Lambda value adapter.
static constexpr unsigned PARTICLE_EMITTER_LIMIT = 32, PARTICLE_STORAGE_LIMIT = 65536, PARTICLE_SPAWN_LIMIT = 4096,
                          PARTICLE_CURVE_LIMIT = SCENE3D_PARTICLE_CURVE_LIMIT, PARTICLE_EVENT_STORAGE_LIMIT = 16384,
                          PARTICLE_SURFACE_STORAGE_LIMIT = 65536;
static constexpr unsigned PARTICLE_COMMAND_LIMIT = 128, PARTICLE_DEPTH_LIMIT = 8;
enum ParticleParameter {
    PARTICLE_RATE,
    PARTICLE_DURATION,
    PARTICLE_LIFE_MIN,
    PARTICLE_LIFE_MAX,
    PARTICLE_SIZE,
    PARTICLE_DRAG,
    PARTICLE_RADIUS,
    PARTICLE_ANGLE,
    PARTICLE_SPEED,
    PARTICLE_SPREAD,
    PARTICLE_PARAMETERS
};
enum ParticleShape { PARTICLE_POINT, PARTICLE_BOX, PARTICLE_SPHERE, PARTICLE_CONE, PARTICLE_MESH };
enum ParticleOverflow { PARTICLE_DROP, PARTICLE_RECYCLE, PARTICLE_ERROR };
struct ParticleCurve {
    unsigned count;
    double times[PARTICLE_CURVE_LIMIT], values[PARTICLE_CURVE_LIMIT * 3];
};
struct ParticleDescription {
    double parameters[PARTICLE_PARAMETERS];
    Scene3dVec extent, velocity, gravity;
    Scene3dParticleCollisionDesc collision;
    Scene3dParticleAnalytic analytic;
    Scene3dParticleNoise noise;
    Scene3dParticleSheet sheet;
    Scene3dParticleTrailDesc trail;
    Scene3dParticleSurface *surface;
    unsigned surface_slot;
    float normal_speed;
    uint64_t seed;
    unsigned capacity, burst, shape, overflow;
    bool looping, emitting, gpu, world, billboard;
    ParticleCurve curves[SCENE3D_PARTICLE_CURVES];
};
struct ParticlePlayback {
    unsigned head, count;
    double time, credit;
    uint64_t sequence, spawned, dropped, recycled, collisions, killed, queries, geometry_work, triangle_work,
        event_sequence;
    unsigned event_head, event_count;
    bool started;
};
struct ParticleRecord {
    Scene3dParticleSample value;
    Scene3dVec dynamic_velocity; // authored transport never accumulates into force-driven motion
    Scene3dVec spawn_velocity; // local recipe offset for device-owned births
    Scene3dMatrix frame; // frozen birth basis/translation; motion is in the selected simulation space
    float initial_size;
    unsigned slot, surface_slot;
    bool newborn;
};
struct ParticleEvent {
    uint64_t sequence, particle, tick, world_identity;
    Scene3dParticleContact contact;
    unsigned depth;
};
struct ParticleSpawnRequest {
    uint64_t emitter;
    Scene3dVec position, velocity;
    unsigned count, depth;
    bool positioned;
};
struct ParticleSpawnBatch { unsigned count; ParticleSpawnRequest requests[PARTICLE_COMMAND_LIMIT]; };
struct ParticleEmitter {
    DomNodeRef source;
    char id[128];
    uint64_t identity;
    ParticleDescription description;
    ParticlePlayback playback;
    ParticleRecord *particles, *scratch;
    ParticleEvent *events, *event_scratch;
    Scene3dMatrix *matrices;
    Scene3dParticleGpu *gpu;
    Scene3dParticleTrailState *trail;
    Scene3dParticleSurfaces surfaces;
    float *slots, *colors, *billboards;
    unsigned *frames;
    Scene3dParticleSheet sheet; // committed presentation, independent of pending source edits
    float max_size;
    unsigned matrix_count;
    bool rotating, fading;
    Scene3dVec lo, hi;
};
struct Scene3dParticles {
    DomDocument *document;
    DomNodeRef root;
    Scene3dRuntime *runtime;
    Scene3dParticleGpuOps gpu_ops;
    Scene3dPhysics *replay_physics;
    Scene3dAnimationState *replay_animation;
    Scene3dRuntime *replay_runtime;
    ParticleEmitter *emitters[PARTICLE_EMITTER_LIMIT];
    unsigned count;
    ParticleSpawnBatch pending;
    unsigned pending_spawns;
    uint64_t emitter_sequence;
    uint64_t epoch, generation, replay_source_generation;
    uint64_t replay_source_epoch, replay_clock_generation, replay_physics_generation, replay_world_identity,
        replay_animation_generation;
    size_t replay_ticks;
    double replay_timestep;
    bool bound, automatic, replay_geometry;
    const char *diagnostic;
};
ParticleEmitter* scene3d_particle_emitter_identity(Scene3dParticles* state, uint64_t identity);
