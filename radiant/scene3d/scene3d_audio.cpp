#include "scene3d_audio.hpp"
#include "scene3d_audio_internal.hpp"
#include "scene3d_source.hpp"
#include "../audio.hpp"
#include "../audio_cues.hpp"
#include "../audio_cue_owner.hpp"
#include "../audio_render.hpp"
#include "../speech.hpp"
#include "../event.hpp"
#include "../../lambda/input/css/dom_element.hpp"
#include "../../lambda/core/mark_reader.hpp"
#include "../../lambda/core/physics_scene.hpp"
#include "../../lib/mem.h"
#include "../../lib/str.h"
#include <float.h>

static constexpr struct { const char* name; float fallback, minimum, maximum; } audio_numbers[] = {
    {"gain", 1, 0, 1}, {"pitch", 1, .125f, 8}, {"smoothing", .01f, 0, 10},
    {"lowpass", AUDIO_BUS_RATE / 2.f, 20, AUDIO_BUS_RATE / 2.f},
    {"ref_distance", 1, .001f, 1e6f}, {"max_distance", 1000, .001f, 1e6f},
    {"rolloff", 1, 0, 8}, {"cone_inner", (float)M_PI, 0, (float)M_PI},
    {"cone_outer", (float)M_PI, 0, (float)M_PI}, {"cone_gain", 0, 0, 1}
};
static_assert(sizeof(audio_numbers) / sizeof(*audio_numbers) == AUDIO_NUMBERS, "audio numeric schema");
static bool audio_fail(Scene3dAudio* state, const char* message, const char* id = nullptr) {
    if (id) snprintf(state->diagnostic, sizeof(state->diagnostic), "audio source '%s': %s", id, message);
    else str_copy(state->diagnostic, sizeof(state->diagnostic), message, strlen(message));
    return false;
}
static bool audio_vector_valid(Scene3dVec value) {
    return isfinite(value.x) && isfinite(value.y) && isfinite(value.z) && fabsf(value.x) <= AUDIO_COORDINATE_LIMIT &&
           fabsf(value.y) <= AUDIO_COORDINATE_LIMIT && fabsf(value.z) <= AUDIO_COORDINATE_LIMIT;
}
static bool audio_vector_equal(Scene3dVec a, Scene3dVec b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static bool audio_motion_equal(const AudioMotion& a, const AudioMotion& b) {
    return audio_vector_equal(a.position, b.position) && audio_vector_equal(a.velocity, b.velocity) &&
        a.time == b.time && a.valid == b.valid && a.reset == b.reset;
}
static bool audio_description_equal(const AudioDescription& a, const AudioDescription& b) {
    return !strcmp(a.id, b.id) && !strcmp(a.clip, b.clip) && !strcmp(a.bus, b.bus) && !memcmp(a.values, b.values, sizeof(a.values)) &&
        audio_vector_equal(a.velocity, b.velocity) && a.spatial == b.spatial && a.distance == b.distance &&
        a.loop == b.loop && a.doppler == b.doppler && a.explicit_velocity == b.explicit_velocity && a.pitch_follow == b.pitch_follow;
}
static bool audio_parameters_equal(const RdtAudioParameters& a, const RdtAudioParameters& b) {
    return a.left == b.left && a.right == b.right && a.pitch == b.pitch && a.smoothing == b.smoothing &&
        a.lowpass == b.lowpass && a.mono == b.mono && a.loop == b.loop;
}
static bool audio_text(DomElement* node, const char* key, const char* fallback, char* out, bool empty = false) {
    const char* value = scene3d_text(node, key);
    if (!value && scene3d_present(node, key)) return false;
    if (!value) value = fallback;
    if (!value || (!empty && !*value) || strlen(value) >= 128) return false;
    str_copy(out, 128, value, strlen(value)); return true;
}
static bool audio_space(DomNode* child) {
    return !child->is_element() && (!child->is_text() || !child->as_text()->native_string ||
        strspn(child->as_text()->native_string->chars, " \t\r\n") == child->as_text()->native_string->len);
}
static bool audio_leaf(DomElement* node) {
    for (DomNode* child = node->first_child; child; child = child->next_sibling)
        if (!audio_space(child)) return false;
    return true;
}
bool scene3d_audio_tag(DomElement* node) {
    return node && (scene3d_tag(node, "audio-source") || scene3d_tag(node, "audio-listener") || scene3d_tag(node, "audio-bus"));
}
bool scene3d_audio_description(DomElement* node, AudioDescription* description) {
    *description = {}; char spatial[128], distance[128];
    if (!audio_text(node, "id", nullptr, description->id) || !audio_text(node, "clip", nullptr, description->clip) ||
        !audio_text(node, "spatial", "3d", spatial) || !audio_text(node, "distance_model", "inverse", distance) ||
        !audio_text(node, "bus", "", description->bus, true) || !audio_leaf(node)) return false;
    if (!strcmp(spatial, "none")) description->spatial = AUDIO_STEREO;
    else if (!strcmp(spatial, "2d")) description->spatial = AUDIO_2D;
    else if (!strcmp(spatial, "3d")) description->spatial = AUDIO_3D;
    else return false;
    if (!strcmp(distance, "inverse")) description->distance = AUDIO_INVERSE;
    else if (!strcmp(distance, "linear")) description->distance = AUDIO_LINEAR;
    else if (!strcmp(distance, "none")) description->distance = AUDIO_NO_DISTANCE;
    else return false;
    for (unsigned i = 0; i < AUDIO_NUMBERS; i++) {
        const auto& spec = audio_numbers[i]; auto& value = description->values[i];
        if (!scene3d_number(node, spec.name, spec.fallback, &value) || value < spec.minimum || value > spec.maximum) return false;
    }
    if (description->values[AUDIO_MAXIMUM] <= description->values[AUDIO_REFERENCE] ||
        description->values[AUDIO_INNER] > description->values[AUDIO_OUTER] ||
        !scene3d_flag(node, "loop", false, &description->loop) || !scene3d_flag(node, "doppler", true, &description->doppler) ||
        !scene3d_flag(node, "pitch_follow", false, &description->pitch_follow) ||
        !scene3d_vector(node, "velocity", {}, &description->velocity) || !audio_vector_valid(description->velocity)) return false;
    description->explicit_velocity = scene3d_present(node, "velocity"); return true;
}
Scene3dVisit scene3d_audio_gather(DomElement* node, void* context) {
    auto* gather = (AudioGather*)context;
    if (node != gather->root && scene3d_tag(node, "scene3d")) return SCENE3D_VISIT_SKIP;
    if (scene3d_tag(node, "audio-bus")) {
        if (gather->bus_count == AUDIO_GRAPH_BUSES) return SCENE3D_VISIT_ERROR;
        gather->buses[gather->bus_count++] = node; return SCENE3D_VISIT_SKIP;
    }
    bool listener = scene3d_tag(node, "audio-listener"), camera = scene3d_tag(node, "camera");
    if (listener) {
        if (gather->explicit_listener || !audio_leaf(node)) return SCENE3D_VISIT_ERROR;
        gather->explicit_listener = node;
    }
    if (camera || listener) {
        const char* id = scene3d_text(node, "id");
        if (gather->listener_id && id && !strcmp(gather->listener_id, id)) {
            if (gather->selected) return SCENE3D_VISIT_ERROR;
            gather->selected = node;
        }
        if (camera && ((gather->camera_id && id && !strcmp(gather->camera_id, id)) || (!gather->camera_id && !gather->camera)))
            gather->camera = node;
    }
    if (scene3d_tag(node, "audio-source")) {
        if (gather->count == AUDIO_SCENE_SOURCES) return SCENE3D_VISIT_ERROR;
        gather->sources[gather->count++] = node; return SCENE3D_VISIT_SKIP;
    }
    return listener ? SCENE3D_VISIT_SKIP : SCENE3D_VISIT_CHILDREN;
}
struct AudioIdCheck { AudioGather* gather; AudioSource* sources; AudioListener* listener; };
static Scene3dVisit audio_check_ids(DomElement* node, void* context) {
    auto& check = *(AudioIdCheck*)context;
    if (node != check.gather->root && scene3d_tag(node, "scene3d")) return SCENE3D_VISIT_SKIP;
    const char* id = scene3d_text(node, "id");
    if (id) {
        for (unsigned i = 0; i < check.gather->count; i++)
            if (node != check.gather->sources[i] && !strcmp(id, check.sources[i].description.id)) return SCENE3D_VISIT_ERROR;
        for (unsigned i = 0; i < check.gather->bus_count; i++) {
            auto* bus = check.gather->buses[i]; const char* bus_id = scene3d_text(bus, "id");
            if (bus_id && node != bus && !strcmp(id, bus_id)) return SCENE3D_VISIT_ERROR;
        }
        if (check.listener->node.address && *check.listener->id && node != check.listener->node.address && !strcmp(id, check.listener->id)) return SCENE3D_VISIT_ERROR;
    }
    return scene3d_audio_tag(node) ? SCENE3D_VISIT_SKIP : SCENE3D_VISIT_CHILDREN;
}
static constexpr AudioEffectField audio_effect_fields[] = {
    {"frequency", offsetof(AudioEffectConfig, frequency)}, {"q", offsetof(AudioEffectConfig, q)},
    {"gain_db", offsetof(AudioEffectConfig, gain_db)}, {"seconds", offsetof(AudioEffectConfig, seconds)},
    {"feedback", offsetof(AudioEffectConfig, feedback)}, {"wet", offsetof(AudioEffectConfig, wet)},
    {"threshold", offsetof(AudioEffectConfig, threshold)}, {"ratio", offsetof(AudioEffectConfig, ratio)},
    {"attack", offsetof(AudioEffectConfig, attack)}, {"release", offsetof(AudioEffectConfig, release)},
    {"makeup", offsetof(AudioEffectConfig, makeup)}, {"ceiling", offsetof(AudioEffectConfig, ceiling)}
};
const AudioEffectField* scene3d_audio_effect_fields(size_t* count) {
    // Parsing and script snapshots share the same authored effect schema.
    *count = sizeof(audio_effect_fields) / sizeof(audio_effect_fields[0]);
    return audio_effect_fields;
}
static bool audio_bus_description(DomElement* node, AudioBusConfig* bus) {
    char id[128];
    if (!audio_text(node, "id", nullptr, id)) return false;
    *bus = audio_graph_bus_default(id);
    if (!audio_text(node, "parent", !strcmp(id, "master") ? "" : "master", bus->parent, true) ||
        !scene3d_number(node, "gain", 1, &bus->gain) || !scene3d_number(node, "smoothing", .01f, &bus->smoothing) ||
        !scene3d_flag(node, "mute", false, &bus->mute) || !scene3d_flag(node, "solo", false, &bus->solo)) return false;
    for (DomNode* child = node->first_child; child; child = child->next_sibling) {
        if (!child->is_element()) { if (!audio_space(child)) return false; continue; }
        auto* element = child->as_element(); if (!audio_leaf(element)) return false;
        if (scene3d_tag(element, "audio-send")) {
            if (bus->send_count == AUDIO_GRAPH_SENDS) return false;
            auto& send = bus->sends[bus->send_count++];
            if (!audio_text(element, "bus", nullptr, send.bus) || !scene3d_number(element, "gain", 1, &send.gain)) return false;
        } else if (scene3d_tag(element, "audio-effect")) {
            if (bus->insert_count == AUDIO_GRAPH_INSERTS) return false;
            const char* name = scene3d_text(element, "type"); AudioEffectKind kind = AUDIO_EFFECT_KINDS;
            for (unsigned i = 0; name && i < AUDIO_EFFECT_KINDS; i++) if (!strcmp(name, audio_effect_name((AudioEffectKind)i))) kind = (AudioEffectKind)i;
            if (kind == AUDIO_EFFECT_KINDS) return false;
            auto& effect = bus->inserts[bus->insert_count++]; effect = audio_effect_default(kind);
            for (const auto& field : audio_effect_fields) {
                float* value = (float*)((char*)&effect + field.offset);
                if (!scene3d_number(element, field.key, *value, value)) return false;
            }
            if (!audio_text(element, "sidechain", "", effect.sidechain, true)) return false;
        } else return false;
    }
    return true;
}
struct AudioPreparedGraph {
    AudioGraph* value = nullptr;
    ~AudioPreparedGraph() { audio_graph_destroy(value); }
};
const char* scene3d_audio_bus_config(const AudioGather& gather, AudioGraphConfig* result) {
    *result = {}; auto& config = *result; config.count = 1; config.buses[0] = audio_graph_bus_default("master");
    bool master = false;
    for (unsigned i = 0; i < gather.bus_count; i++) {
        AudioBusConfig bus;
        if (!audio_bus_description(gather.buses[i], &bus)) return "audio graph: invalid bus/insert description";
        if (!strcmp(bus.id, "master")) {
            if (master) return "audio graph: duplicate master";
            master = true; config.buses[0] = bus;
        } else {
            if (config.count == AUDIO_GRAPH_BUSES) return "audio graph: master plus buses exceeds limit";
            config.buses[config.count++] = bus;
        }
    }
    return nullptr;
}
static bool audio_prepare_routing(Scene3dAudio* state, const AudioGather& gather, AudioSource* sources, AudioPreparedGraph* prepared) {
    if (gather.bus_count || state->graph) {
        AudioGraphConfig config;
        if (const char* error = scene3d_audio_bus_config(gather, &config)) return audio_fail(state, error);
        if (!state->graph) {
            state->submix = radiant_document_submix(state->document);
            state->graph = audio_submix_graph_open(state->submix);
        }
        const char* error = nullptr;
        if (!state->graph || !audio_submix_graph_prepare(state->submix, state->graph, config, &prepared->value, &error))
            return audio_fail(state, error ? error : "audio graph: owner admission failed");
        if (prepared->value && state->routing_generation == UINT64_MAX) return audio_fail(state, "audio graph: generation exhausted");
        if (prepared->value && !radiant_audio_graph_may_edit(state->document, state->graph))
            return audio_fail(state, "audio graph: active input/tail change requires document activation");
    }
    for (unsigned i = 0; i < gather.count; i++) {
        auto& source = sources[i]; const char* bus = *source.description.bus ? source.description.bus : "master";
        auto before = source.route;
        source.route = state->graph ? audio_submix_graph_route(state->submix, state->graph, prepared->value, bus) : 0;
        if ((state->graph && !source.route) || (!state->graph && strcmp(bus, "master"))) return audio_fail(state, "audio graph: unknown source bus", source.description.id);
        RdtAudioInfo info = {};
        if (source.player && (prepared->value || source.route != before) &&
            radiant_audio_info(state->document, source.player, &info) && (info.state == RDT_AUDIO_PLAYING || info.armed) &&
            info.gain > 0 && !radiant_document_may_start_audio(state->document)) return audio_fail(state, "audio graph: active route change requires document activation");
    }
    return true;
}
static bool audio_motion(AudioMotion* motion, Scene3dVec position, Scene3dVec supplied, bool explicit_velocity, double time) {
    if (!audio_vector_valid(position) || !audio_vector_valid(supplied) || !isfinite(time)) return false;
    Scene3dVec velocity = explicit_velocity ? supplied : Scene3dVec{};
    if (!explicit_velocity && motion->valid && time > motion->time && !motion->reset) {
        double delta = time - motion->time;
        double values[] = {((double)position.x - motion->position.x) / delta,
                           ((double)position.y - motion->position.y) / delta,
                           ((double)position.z - motion->position.z) / delta};
        for (double value : values) if (!isfinite(value) || fabs(value) > AUDIO_COORDINATE_LIMIT) return false;
        velocity = {(float)values[0], (float)values[1], (float)values[2]};
    } else if (!explicit_velocity && motion->valid && time == motion->time &&
               audio_vector_equal(position, motion->position) && !motion->reset) velocity = motion->velocity;
    bool reset = motion->reset && motion->valid && time <= motion->time;
    *motion = {position, velocity, time, true, reset}; return true;
}
static bool audio_world(Scene3dAudio* state, DomElement* root, DomElement* node, Scene3dMatrix* world) {
    bool visible;
    return scene3d_world_pose(root, node, state->animation, scene3d_override_physics_pose, state->physics, world, &visible);
}
bool scene3d_audio_listener_frame(Scene3dAudio* state, DomElement* root, AudioListener* listener, double time) {
    if (!listener->node.address) return true;
    auto* node = dom_node_ref_validate(state->document, listener->node);
    if (!node || !node->is_element() || !node->parent || !node->parent->is_element()) return false;
    Scene3dMatrix world, parent, view; Scene3dVec target, up;
    if (!audio_world(state, root, node->as_element(), &world) || !audio_world(state, root, node->parent->as_element(), &parent) ||
        !scene3d_camera_targets(node->as_element(), parent, world, &target, &up)) return false;
    auto position = scene3d_point(world, {});
    if (!audio_motion(&listener->motion, position, listener->velocity, listener->explicit_velocity, time) ||
        !scene3d_camera(position, target, up, &view)) return false;
    listener->right = {view.v[0], view.v[4], view.v[8]};
    listener->forward = {-view.v[2], -view.v[6], -view.v[10]}; return true;
}
bool scene3d_audio_source_frame(Scene3dAudio* state, DomElement* root, AudioSource* source, const AudioListener& listener,
    float speed, double time) {
    auto* node = dom_node_ref_validate(state->document, source->node); Scene3dMatrix world;
    if (!node || !node->is_element() || !audio_world(state, root, node->as_element(), &world)) return false;
    const auto& d = source->description;
    if (!audio_motion(&source->motion, scene3d_point(world, {}), d.velocity, d.explicit_velocity, time)) return false;
    auto parameters = rdt_audio_default_parameters(); parameters.loop = d.loop;
    parameters.smoothing = d.values[AUDIO_SMOOTH]; parameters.lowpass = d.values[AUDIO_LOWPASS]; parameters.pitch = d.values[AUDIO_PITCH];
    if (d.spatial != AUDIO_STEREO) {
        if (!listener.node.address) return false;
        Scene3dVec delta = scene3d_sub(source->motion.position, listener.motion.position), right = listener.right;
        Scene3dVec source_velocity = source->motion.velocity, listener_velocity = listener.motion.velocity;
        if (d.spatial == AUDIO_2D) {
            delta.z = right.z = source_velocity.z = listener_velocity.z = 0;
            if (!scene3d_normalize(&right)) return false;
        }
        double distance = sqrt((double)delta.x * delta.x + (double)delta.y * delta.y + (double)delta.z * delta.z);
        Scene3dVec direction = distance > 0 ? scene3d_scale(delta, (float)(1 / distance)) : Scene3dVec{};
        double range = fmin(d.values[AUDIO_MAXIMUM], fmax(d.values[AUDIO_REFERENCE], distance));
        double gain = 1;
        if (d.distance == AUDIO_INVERSE) gain = d.values[AUDIO_REFERENCE] /
            (d.values[AUDIO_REFERENCE] + d.values[AUDIO_ROLLOFF] * (range - d.values[AUDIO_REFERENCE]));
        else if (d.distance == AUDIO_LINEAR) gain = fmax(0, 1 - d.values[AUDIO_ROLLOFF] *
            (range - d.values[AUDIO_REFERENCE]) / (d.values[AUDIO_MAXIMUM] - d.values[AUDIO_REFERENCE]));
        if (distance > 0 && d.values[AUDIO_INNER] < (float)M_PI && d.values[AUDIO_CONE_GAIN] < 1) {
            auto forward = scene3d_point(world, d.spatial == AUDIO_2D ? Scene3dVec{1, 0, 0} : Scene3dVec{0, 0, -1}, 0);
            if (d.spatial == AUDIO_2D) forward.z = 0;
            if (!scene3d_normalize(&forward)) return false;
            double angle = acos(fmax(-1, fmin(1, -scene3d_dot(forward, direction))));
            double blend = angle <= d.values[AUDIO_INNER] ? 0 : angle >= d.values[AUDIO_OUTER] ? 1 :
                (angle - d.values[AUDIO_INNER]) / (d.values[AUDIO_OUTER] - d.values[AUDIO_INNER]);
            gain *= 1 + (d.values[AUDIO_CONE_GAIN] - 1) * blend;
        }
        double pan = fmax(-1, fmin(1, scene3d_dot(direction, right))), angle = (pan + 1) * M_PI / 4;
        parameters.left = (float)(gain * cos(angle)); parameters.right = (float)(gain * sin(angle)); parameters.mono = true;
        if (d.doppler && distance > 0) {
            double source_speed = fmax(-.95 * speed, fmin(.95 * speed, scene3d_dot(source_velocity, direction)));
            double listener_speed = fmax(-.95 * speed, fmin(.95 * speed, scene3d_dot(listener_velocity, direction)));
            parameters.pitch = (float)fmax(.125, fmin(8, parameters.pitch * (speed + listener_speed) / (speed + source_speed)));
        }
    }
    source->unscaled_pitch = parameters.pitch;
    if (d.pitch_follow) {
        Scene3dRuntimeStats clock = {}; scene3d_runtime_stats(state->runtime, &clock);
        parameters.pitch = (float)fmax(.125, fmin(8, parameters.pitch * clock.time_scale));
    }
    source->parameters = parameters; return true;
}
static bool audio_publish(Scene3dAudio* state, AudioSource* sources, unsigned count, AudioListener* listener,
    float speed, double time) {
    auto* root = dom_node_ref_validate(state->document, state->root);
    if (!root || !root->is_element() || !scene3d_audio_listener_frame(state, root->as_element(), listener, time))
        return audio_fail(state, "audio: invalid sampled listener frame or velocity");
    for (unsigned i = 0; i < count; i++) {
        if (!scene3d_audio_source_frame(state, root->as_element(), sources + i, *listener, speed, time))
            return audio_fail(state, "invalid sampled frame, cone or velocity", sources[i].description.id);
        RdtAudioInfo info = {};
        if (sources[i].player && radiant_audio_info(state->document, sources[i].player, &info) &&
            (info.state == RDT_AUDIO_PLAYING || info.armed) && sources[i].description.values[AUDIO_GAIN] > 0 &&
            sources[i].description.values[AUDIO_GAIN] != info.gain && !radiant_document_may_start_audio(state->document))
            return audio_fail(state, "audible gain change requires document activation", sources[i].description.id);
    }
    if (state->sample_sequence == UINT64_MAX) return audio_fail(state, "audio: sample sequence exhausted");
    bool changed = !audio_motion_equal(listener->motion, state->listener.motion) ||
        !audio_vector_equal(listener->right, state->listener.right) || !audio_vector_equal(listener->forward, state->listener.forward) || count != state->count;
    for (unsigned i = 0; i < count; i++) {
        auto& source = sources[i];
        changed |= i >= state->count || !audio_motion_equal(source.motion, state->sources[i].motion) ||
                   !audio_parameters_equal(source.parameters, state->sources[i].parameters);
        if (source.player) {
            if (!radiant_audio_parameters(state->document, source.player, &source.parameters, source.description.values[AUDIO_GAIN])) {
                RdtAudioInfo info = {};
                if (radiant_audio_info(state->document, source.player, &info))
                    return audio_fail(state, "native parameter publication failed", source.description.id);
                source.player = 0;
            }
        }
    }
    if (changed) state->sample_sequence++;
    return true;
}
Scene3dAudio* scene3d_audio_create(DomElement* root, Scene3dRuntime* runtime) {
    if (!root || !runtime) return nullptr;
    auto* state = (Scene3dAudio*)mem_calloc(1, sizeof(Scene3dAudio), MEM_CAT_RENDER);
    if (state) { state->document = root->doc; state->root = dom_node_ref(root); state->runtime = runtime;
        state->generation = 1; state->epoch = UINT64_MAX; state->speed = 343; state->cue_epoch = 1; }
    return state;
}
void scene3d_audio_cues_collect(Scene3dAudio* state) {
    if (!state->cues || !audio_submix_lock(state->submix)) return;
    bool scene_work = audio_cues_scene_work_locked(state->cues);
    audio_submix_unlock(state->submix);
    audio_cue_owner_collect(state->cue_owner);
    scene3d_runtime_clock_work(state->runtime, scene_work);
}
const char* scene3d_audio_cues_sync(Scene3dAudio* state) {
    if (!state->cues) return nullptr;
    scene3d_audio_cues_collect(state);
    // a held queue can be the first new work after a long idle interval; never consume that idle time.
    if (!audio_submix_has_work(state->submix)) radiant_document_submix(state->document);
    Scene3dRuntimeStats clock = {}; scene3d_runtime_stats(state->runtime, &clock);
    if (!audio_submix_lock(state->submix)) return "audio cues: document submix retired";
    audio_cues_clock_locked(state->cues, audio_submix_clock(state->submix), scene3d_runtime_sampled_time(state->runtime), clock.time_scale, clock.paused || clock.faulted);
    bool work = audio_cues_next_locked(state->cues) != UINT64_MAX;
    audio_submix_unlock(state->submix);
    const char* error = nullptr;
    if (!audio_submix_prepare_output(state->submix, work, &error)) {
        if (audio_submix_lock(state->submix)) {
            audio_cues_cancel_locked(state->cues, 0, audio_submix_clock(state->submix)); audio_submix_unlock(state->submix);
        }
        return error ? error : "audio cues: output admission failed";
    }
    return nullptr;
}
void scene3d_audio_reset(Scene3dAudio* state) {
    if (!state) return;
    if (state->cue_epoch) state->cue_epoch = state->cue_epoch == UINT64_MAX ? 0 : state->cue_epoch + 1;
    if (state->cues && audio_submix_lock(state->submix)) {
        audio_cues_reset_locked(state->cues, state->cue_epoch, audio_submix_clock(state->submix)); audio_submix_unlock(state->submix);
    }
    for (unsigned i = 0; i < state->count; i++) {
        radiant_audio_close(state->document, state->sources[i].player);
        state->sources[i].player = 0; state->sources[i].motion = {};
    }
    state->listener.motion = {};
    scene3d_audio_cues_collect(state);
    audio_submix_graph_reset(state->submix, state->graph);
}
void scene3d_audio_destroy(Scene3dAudio* state) {
    if (!state) return;
    Scene3dRuntimeStats clock = {}; scene3d_runtime_stats(state->runtime, &clock);
    audio_render_close_session(state->document, clock.session);
    scene3d_audio_reset(state);
    audio_cue_owner_destroy(state->cue_owner); state->cue_owner = nullptr; state->cues = nullptr;
    audio_submix_graph_close(state->submix, state->graph);
    for (unsigned i = 0; i < state->clip_count; i++) radiant_audio_close(state->document, state->clips[i].token);
    mem_free(state);
}
const char* scene3d_audio_listener_description(Scene3dAudio* state, const AudioGather& gather, AudioListener* result) {
    auto& listener = *result;
    DomElement* selected = gather.listener_id ? gather.selected : gather.explicit_listener ? gather.explicit_listener : gather.camera;
    if (gather.listener_id && !selected) return "audio: missing selected listener";
    if (selected) {
        auto ref = dom_node_ref(selected);
        if (state->listener.node.address == ref.address && state->listener.node.expected_id == ref.expected_id) listener = state->listener;
        listener.node = ref; listener.camera = scene3d_tag(selected, "camera");
        const char* velocity = listener.camera ? "audio_velocity" : "velocity";
        if (!audio_text(selected, "id", "", listener.id, true) || !scene3d_vector(selected, velocity, {}, &listener.velocity) ||
            !audio_vector_valid(listener.velocity)) return "audio: invalid listener description";
        listener.explicit_velocity = scene3d_present(selected, velocity);
    }
    return nullptr;
}
bool scene3d_audio_prepare(Scene3dAudio* state, DomElement* root, Scene3dAnimationState* animation,
    Scene3dPhysics* physics, char* diagnostic, size_t capacity) {
    auto fail = [&](const char* message) { audio_fail(state, message); str_copy(diagnostic, capacity, state->diagnostic, strlen(state->diagnostic)); return false; };
    if (!state || !root || root->doc != state->document || !dom_element_is_connected(root)) return false;
    state->animation = animation; state->physics = physics;
    if (state->epoch == root->doc->mutation_epoch) {
        const char* error = scene3d_audio_sample(state, scene3d_runtime_sampled_time(state->runtime));
        if (error) { str_copy(diagnostic, capacity, error, strlen(error)); return false; }
        return true;
    }
    AudioGather gather{root, {}, nullptr, nullptr, nullptr, 0, scene3d_text(root, "audio_listener"), scene3d_text(root, "camera")};
    if ((scene3d_present(root, "audio_listener") && !gather.listener_id) ||
        !scene3d_visit(root, scene3d_audio_gather, &gather)) return fail("audio: invalid source/listener hierarchy or source limit");
    AudioSource sources[AUDIO_SCENE_SOURCES] = {}; AudioListener listener = {};
    if (const char* error = scene3d_audio_listener_description(state, gather, &listener)) return fail(error);
    uint64_t identity = state->identity;
    for (unsigned i = 0; i < gather.count; i++) {
        auto ref = dom_node_ref(gather.sources[i]);
        for (unsigned j = 0; j < state->count; j++)
            if (state->sources[j].node.address == ref.address && state->sources[j].node.expected_id == ref.expected_id) sources[i] = state->sources[j];
        AudioDescription description;
        if (!scene3d_audio_description(gather.sources[i], &description)) return fail("audio: invalid source description");
        if (!sources[i].identity || strcmp(sources[i].description.id, description.id)) {
            if (identity == UINT64_MAX) return fail("audio: source identity exhausted");
            sources[i] = {}; sources[i].identity = ++identity;
        }
        sources[i].node = ref; sources[i].description = description;
    }
    AudioIdCheck ids{&gather, sources, &listener};
    if (!scene3d_visit(root, audio_check_ids, &ids)) return fail("audio: source/listener ID conflicts with scene content");
    AudioPreparedGraph prepared;
    if (!audio_prepare_routing(state, gather, sources, &prepared)) {
        str_copy(diagnostic, capacity, state->diagnostic, strlen(state->diagnostic)); return false;
    }
    float speed;
    if (!scene3d_number(root, "audio_speed_of_sound", 343, &speed) || speed < .001f || speed > AUDIO_COORDINATE_LIMIT ||
        state->generation == UINT64_MAX) return fail("audio: invalid speed of sound or exhausted generation");
    if (!audio_publish(state, sources, gather.count, &listener, speed, scene3d_runtime_sampled_time(state->runtime))) {
        str_copy(diagnostic, capacity, state->diagnostic, strlen(state->diagnostic)); return false;
    }
    bool routing_changed = prepared.value != nullptr;
    if (state->graph) {
        AudioPlayerRoute routes[AUDIO_SCENE_SOURCES]; unsigned count = 0;
        for (unsigned i = 0; i < gather.count; i++) if (sources[i].player) routes[count++] = {sources[i].player, sources[i].route};
        const char* error = nullptr;
        if (!radiant_audio_graph_commit(state->document, state->graph, prepared.value, routes, count, &error))
            return fail(error ? error : "audio graph: native publication rejected");
        prepared.value = nullptr;
        if (routing_changed) state->routing_generation++;
    }
    for (unsigned i = 0; i < state->count; i++) {
        bool retained = false;
        for (unsigned j = 0; j < gather.count; j++) if (state->sources[i].identity == sources[j].identity) retained = true;
        if (!retained) radiant_audio_close(state->document, state->sources[i].player);
    }
    bool changed = state->count != gather.count || state->speed != speed || routing_changed;
    for (unsigned i = 0; i < gather.count; i++) changed |= i >= state->count ||
        !audio_description_equal(sources[i].description, state->sources[i].description) || sources[i].identity != state->sources[i].identity;
    changed |= state->listener.node.address != listener.node.address || state->listener.node.expected_id != listener.node.expected_id;
    memcpy(state->sources, sources, gather.count * sizeof(*sources)); state->count = gather.count;
    state->listener = listener; state->identity = identity; state->speed = speed;
    if (changed) state->generation++;
    state->epoch = root->doc->mutation_epoch; state->diagnostic[0] = 0; return true;
}
const char* scene3d_audio_sample(Scene3dAudio* state, double time) {
    if (!state) return nullptr;
    if (const char* error = scene3d_audio_cues_sync(state)) { audio_fail(state, error); return state->diagnostic; }
    AudioSource sources[AUDIO_SCENE_SOURCES]; memcpy(sources, state->sources, state->count * sizeof(*sources));
    auto listener = state->listener;
    if (!audio_publish(state, sources, state->count, &listener, state->speed, time)) return state->diagnostic;
    memcpy(state->sources, sources, state->count * sizeof(*sources)); state->listener = listener;
    state->diagnostic[0] = 0; return nullptr;
}
void scene3d_audio_follow_rebuild(Scene3dAudio* state, DomNode* before, DomNode* after) {
    if (!state) return;
    view_state_ref_follow_rebuild(state->document, &state->root, before, after);
    view_state_ref_follow_rebuild(state->document, &state->listener.node, before, after);
    for (unsigned i = 0; i < state->count; i++) view_state_ref_follow_rebuild(state->document, &state->sources[i].node, before, after);
    state->epoch = UINT64_MAX;
}
const char* scene3d_audio_diagnostic(Scene3dAudio* state) { return state && state->diagnostic[0] ? state->diagnostic : nullptr; }
