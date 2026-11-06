#include "../../lambda/input/css/dom_element.hpp"
#include "../../lambda/input/css/dom_lifecycle.hpp"
// establish parser entry points' C linkage before auxiliary declarations.
#include "../../lambda/input/input.hpp"
#include "../../lambda/input/input-parsers.h"
#include "../../lambda/core/navigation_graph.hpp"
#include "../../lib/mem.h"
#include "../../lib/physics/physics_math.h"
#include "../../lib/thread_pool.h"
#include "../event.hpp"
#include "scene3d.hpp"
#include "scene3d_navigation_internal.hpp"
#include "scene3d_navigation_event_host.hpp"
#include "scene3d_physics.hpp"
#include "scene3d_source.hpp"
#include <float.h>
#include <math.h>

Scene3dTickResult scene3d_nav_agents_tick(void* owner, double delta);
bool scene3d_navigation_body_owned(Scene3dNavigation* state, DomElement* body) {
    if (state && body) {
        for (auto* agent = state->agents; agent; agent = agent->next) {
            if (!dom_node_ref_identity_compare(agent->body, dom_node_ref(body))) {
                return true;
            }
        }
    }
    return false;
}
bool scene3d_nav_agents_schedule(Scene3dNavigation* state, DomElement* root);
void scene3d_nav_agent_destroy(Scene3dNavigationAgent* agent) {
    if (agent) {
        agent->~Scene3dNavigationAgent();
        mem_free(agent);
    }
}
void scene3d_nav_job_destroy(Scene3dBakeJob* job) {
    if (job) {
        job->~Scene3dBakeJob();
        mem_free(job);
    }
}
void scene3d_nav_job_stale(Scene3dBakeJob* job, const char* message) {
    if (job->published_generation)
        return;
    job->stale = true;
    str_copy(job->diagnostic, sizeof(job->diagnostic), message, strlen(message));
    __atomic_store_n(&job->worker.cancelled, true, __ATOMIC_RELAXED);
}
static size_t nav_source_views(const Scene3dNavigationAsset* asset, Scene3dNavigationSelection* sources) {
    size_t i = 0;
    for (const auto& source : asset->sources)
        sources[i++] = {source.id, source.walkable, source.groups, source.cost};
    return i;
}
static bool nav_source_key(const Scene3dNavigationAsset* asset, DomElement* root, uint64_t* key, char* diagnostic,
                           size_t capacity) {
    Scene3dNavigationSelection sources[64];
    size_t count = nav_source_views(asset, sources);
    return scene3d_navigation_geometry_key(root, sources, count, key, diagnostic, capacity);
}
bool scene3d_nav_source_snapshot(const Scene3dNavigationAsset* asset, DomElement* root,
                                Scene3dNavigationGeometry* geometry, lam::ArrayList<DomNodeRef>* refs, char* diagnostic,
                                size_t capacity) {
    Scene3dNavigationSelection sources[64];
    size_t count = nav_source_views(asset, sources);
    return scene3d_collect_navigation_geometry(root, sources, count, geometry, refs, diagnostic, capacity);
}
bool scene3d_nav_asset_ready(const Scene3dNavigationAsset* asset) {
    return !asset->source_bake || asset->compiled.bake;
}
size_t scene3d_nav_storage_nodes(const NavigationCompiled& compiled) {
    if (!compiled.bake)
        return navigation_node_count(compiled.graph);
    // merged polygons still retain their backing cells; graph compression cannot expand the cache budget.
    NavigationBakeStats stats = {};
    navigation_bake_stats(compiled.bake, &stats);
    return stats.cells + 2 * navigation_links_count(compiled.links);
}
NavigationBakeStats scene3d_nav_retained_bake(const NavigationCompiled& compiled) {
    NavigationBakeStats stats = {};
    navigation_bake_stats(compiled.bake, &stats);
    return stats;
}
bool scene3d_nav_counts_fit(size_t nodes, size_t edges, size_t vertices, size_t storage_nodes, size_t raw_cells,
                           size_t source_triangles) {
    return nodes <= NAVIGATION_MAX_NODES && edges <= NAVIGATION_MAX_EDGES && vertices <= NAVIGATION_MAX_VERTICES &&
           storage_nodes <= NAVIGATION_MAX_NODES && raw_cells <= NAVIGATION_MAX_NODES &&
           source_triangles <= SCENE3D_BAKE_TRIANGLE_LIMIT;
}
bool scene3d_navigation_tag(DomElement* node) {
    const char* tag = node->local_name();
    return !strncmp(tag, "navigation-", 11) && navigation_kind(tag) != NAVIGATION_INVALID;
}
static void nav_asset_destroy(Scene3dNavigationAsset* asset) {
    if (asset) {
        asset->~Scene3dNavigationAsset();
        mem_free(asset);
    }
}
Scene3dNavigation* scene3d_navigation_create(DomElement* root, Scene3dRuntime* runtime) {
    uint64_t* sequence = scene3d_navigation_sequence(root->doc);
    if (!sequence)
        return nullptr;
    void* memory = mem_alloc(sizeof(Scene3dNavigation), MEM_CAT_RENDER);
    if (!memory)
        return nullptr;
    auto* state = new (memory) Scene3dNavigation();
    state->document = root->doc;
    state->root = dom_node_ref(root);
    state->runtime = runtime;
    state->next_generation = sequence;
    return state;
}
void scene3d_navigation_destroy(Scene3dNavigation* state) {
    if (!state)
        return;
    scene3d_runtime_unbind(state->runtime, SCENE3D_TICK_NAVIGATION, state);
    while (state->agents) {
        auto* agent = state->agents;
        state->agents = agent->next;
        scene3d_nav_agent_destroy(agent);
    }
    for (auto* job = state->jobs; job; job = job->next)
        __atomic_store_n(&job->worker.cancelled, true, __ATOMIC_RELAXED);
    // joining precedes any native input/result teardown; workers never borrow the document.
    tp_destroy(state->pool);
    while (state->jobs) {
        auto* job = state->jobs;
        state->jobs = job->next;
        scene3d_nav_job_destroy(job);
    }
    while (state->assets) {
        auto* asset = state->assets;
        state->assets = asset->next;
        nav_asset_destroy(asset);
    }
    state->~Scene3dNavigation();
    mem_free(state);
}
static bool nav_asset_placement(DomElement* root, DomElement* node, unsigned depth, unsigned* work) {
    if (depth > 64 || ++*work > 8192)
        return false;
    if (scene3d_navigation_tag(node) &&
        (!node->parent || !node->parent->is_element() ||
         strcmp(node->parent->as_element()->local_name(), "resources") || node->parent->parent != root))
        return false;
    for (DomNode* child = node->first_child; child; child = child->next_sibling)
        if (child->is_element() && !nav_asset_placement(root, child->as_element(), depth + 1, work))
            return false;
    return true;
}
static bool nav_source_compile(Scene3dNavigationAsset* asset, DomElement* root, ItemReader source, char* diagnostic,
                               size_t capacity) {
    auto fail = [&](const char* message) {
        str_copy(diagnostic, capacity, message, strlen(message));
        return false;
    };
    auto list = navigation_field(source, "sources");
    NavigationError error = {};
    double version;
    const char* kind = source.isElement() ? source.asElement().tagName() : navigation_text(source.asMap().get("type"));
    if (navigation_kind(kind) != NAVIGATION_BAKE ||
        !navigation_number(navigation_field(source, "version"), 1, 1, &version) || version != 1 ||
        !navigation_field(source, "triangles").isNull() || !navigation_field(source, "format").isNull() ||
        !list.isArray() || !list.asArray().length() || list.asArray().length() > 64 ||
        !navigation_bake_settings_read(navigation_field(source, "settings"), &asset->settings))
        return fail("navigation: source bake requires schema 1, settings and 1..64 selections");
    if (!navigation_bake_settings_valid(&asset->settings, nullptr, nullptr, &error))
        return fail(error.message);
    for (int64_t i = 0; i < list.asArray().length(); i++) {
        auto value = list.asArray().get(i);
        const char* id = navigation_text(value.isMap() ? value.asMap().get("id") : value);
        Scene3dNavigationSource entry;
        size_t groups;
        if (!id || !*id || strlen(id) >= sizeof(entry.id) ||
            !navigation_boolean(value.asMap().get("walkable"), true, &entry.walkable) ||
            !navigation_integer(value.asMap().get("groups"), UINT32_MAX, UINT32_MAX, &groups) ||
            !navigation_number(value.asMap().get("cost"), 1, DBL_MAX, &entry.cost))
            return fail("navigation: invalid source selection");
        for (const auto& before : asset->sources)
            if (!strcmp(before.id, id))
                return fail("navigation: duplicate source selection");
        strcpy(entry.id, id);
        entry.groups = (uint32_t)groups;
        if (!asset->sources.append(entry))
            return fail("navigation: selection allocation failed");
    }
    if (!nav_source_key(asset, root, &asset->geometry_key, diagnostic, capacity))
        return false;
    asset->source_bake = true;
    asset->compiled.kind = NAVIGATION_BAKE;
    asset->compiled.dimensions = 3;
    if (!navigation_links_read(&asset->compiled, source))
        return fail(asset->compiled.diagnostic);
    return navigation_bake_settings_hash(&asset->settings, asset->compiled.settings_hash) ||
           fail("navigation: settings fingerprint failed");
}
static bool nav_source_definition_equal(const Scene3dNavigationAsset* a, const Scene3dNavigationAsset* b) {
    if (!a->source_bake || !b->source_bake)
        return false;
    if (strcmp(a->compiled.settings_hash, b->compiled.settings_hash) || a->sources.size() != b->sources.size())
        return false;
    for (size_t i = 0; i < a->sources.size(); i++) {
        const auto &x = a->sources[i], &y = b->sources[i];
        if (strcmp(x.id, y.id) || x.walkable != y.walkable || x.groups != y.groups || x.cost != y.cost)
            return false;
    }
    return true;
}
static bool nav_asset_equal(const Scene3dNavigationAsset* a, const Scene3dNavigationAsset* b) {
    if (a->source_bake != b->source_bake)
        return false;
    if (!a->source_bake)
        return navigation_compiled_equal(a->compiled, b->compiled);
    return nav_source_definition_equal(a, b) && navigation_link_declarations_equal(a->compiled, b->compiled);
}
static bool nav_asset_compile(Scene3dNavigationAsset* asset, DomElement* root, DomElement* node, char* diagnostic,
                              size_t capacity) {
    auto fail = [&](const char* message) {
        str_copy(diagnostic, capacity, message, strlen(message));
        return false;
    };
    const char* transforms[] = {"position", "rotation", "quaternion", "scale", "matrix"};
    for (const char* key : transforms)
        if (node->has_attribute(key) || !scene3d_value(node, key).isNull())
            return fail("navigation: assets use world coordinates and cannot have object transforms");
    for (DomNode* child = node->first_child; child; child = child->next_sibling)
        if (child->is_element() ||
            (child->is_text() && child->as_text()->native_string &&
             strspn(child->as_text()->native_string->chars, " \r\n\t") != child->as_text()->native_string->len))
            return fail("navigation: assets have no child content");
    double version;
    if (!scene3d_real(node, "version", 1, &version) || version != 1)
        return fail("navigation: asset requires schema version 1");
    struct Scratch {
        Pool* pool = nullptr;
        ~Scratch() {
            if (pool)
                pool_destroy(pool);
        }
    } scratch;
    auto data = scene3d_value(node, "data");
    ItemReader source;
    const char* json = data.isString() ? data.asString()->chars : data.isNull() ? scene3d_text(node, "data") : nullptr;
    if (!data.isNull() || json) {
        if (json) {
            if (strlen(json) > 16 * 1024 * 1024 || (data.isString() && strlen(json) != data.asString()->len))
                return fail("navigation: asset JSON byte quota exceeded or embedded NUL");
            scratch.pool = pool_create();
            if (!scratch.pool)
                return fail("navigation: JSON scratch allocation failed");
            Input* input = Input::create(scratch.pool);
            if (!input)
                return fail("navigation: JSON input allocation failed");
            bool valid = false;
            Item parsed = parse_json_to_item_strict(input, json, &valid);
            if (!valid || get_type_id(parsed) != LMD_TYPE_MAP)
                return fail("navigation: data requires a JSON object");
            source = ItemReader(parsed.to_const());
        } else if (data.isMap())
            source = data;
        else
            return fail("navigation: data requires a map or JSON object string");
    } else {
        Element* backing = dom_element_backing(node);
        if (!backing)
            return fail("navigation: asset has no authored data");
        source = ItemReader(Item{.element = backing}.to_const());
    }
    if (!navigation_field(source, "sources").isNull()) {
        if (strcmp(node->local_name(), "navigation-bake"))
            return fail("navigation: only bake assets accept source selections");
        return nav_source_compile(asset, root, source, diagnostic, capacity);
    }
    if (!navigation_compile(&asset->compiled, source))
        return fail(asset->compiled.diagnostic);
    if (strcmp(node->local_name(), navigation_kind_tag(asset->compiled.kind)))
        return fail("navigation: asset tag and data kind disagree");
    return true;
}
bool scene3d_navigation_prepare(Scene3dNavigation* state, DomElement* root, char* diagnostic, size_t capacity) {
    if (!state || state->document != root->doc)
        return false;
    auto report = [&]() {
        str_copy(diagnostic, capacity, state->diagnostic, strlen(state->diagnostic));
        return false;
    };
    if (state->epoch == root->doc->mutation_epoch)
        return scene3d_nav_agents_schedule(state, root);
    if (state->rejected_epoch == root->doc->mutation_epoch)
        return report();
    auto reject = [&](const char* message) {
        str_copy(state->diagnostic, sizeof(state->diagnostic), message, strlen(message));
        state->rejected_epoch = root->doc->mutation_epoch;
        return report();
    };
    Scene3dRuntimeStats clock = {};
    scene3d_runtime_stats(state->runtime, &clock);
    if (!scene3d_runtime_can_configure(state->runtime, clock.timestep, 1)) {
        // sampling is transient; retry this same authored epoch after the tick finishes.
        const char* message = "navigation: authored edits are unavailable during tick sampling";
        str_copy(diagnostic, capacity, message, strlen(message));
        return false;
    }
    unsigned work = 0;
    if (!nav_asset_placement(root, root, 0, &work))
        return reject("navigation: assets belong directly to scene resources; scene depth/node quota exceeded");
    struct Plan {
        Scene3dNavigationAsset* chosen;
        Scene3dNavigationAsset* created;
        DomNodeRef source;
        uint64_t key;
        uint32_t revision;
        uint64_t geometry_key;
    };
    struct Plans {
        lam::ArrayList<Plan> values;
        ~Plans() {
            for (const auto& plan : values)
                nav_asset_destroy(plan.created);
        }
    } plans;
    size_t nodes = 0, edges = 0, vertices = 0, storage_nodes = 0, raw_cells = 0, source_triangles = 0;
    uint64_t next_generation = *state->next_generation;
    for (DomNode* resource = root->first_child; resource; resource = resource->next_sibling) {
        if (!resource->is_element() || strcmp(resource->as_element()->local_name(), "resources"))
            continue;
        for (DomNode* child = resource->as_element()->first_child; child; child = child->next_sibling) {
            if (!child->is_element() || !scene3d_navigation_tag(child->as_element()))
                continue;
            auto* node = child->as_element();
            const char* id = scene3d_text(node, "id");
            if (!id || !*id || strlen(id) >= 128 || plans.values.size() >= 64)
                return reject("navigation: assets require IDs shorter than 128 bytes and at most 64 definitions");
            for (const auto& plan : plans.values)
                if (!strcmp(plan.chosen->id, id))
                    return reject("navigation: duplicate asset ID");
            uint64_t key = 5381;
            unsigned work = 0;
            if (!scene3d_source_key(node, &key, &work, 0, false))
                return reject("navigation: source quota exceeded");
            double revision;
            if (!scene3d_real(node, "revision", 0, &revision) || revision < 0 || revision > UINT32_MAX ||
                floor(revision) != revision)
                return reject("navigation: invalid asset revision");
            auto ref = dom_node_ref(node);
            Scene3dNavigationAsset* before = nullptr;
            for (auto* asset = state->assets; asset; asset = asset->next)
                if (!strcmp(asset->id, id)) {
                    before = asset;
                    break;
                }
            bool same =
                before && before->source.address == ref.address && before->source.expected_id == ref.expected_id;
            Scene3dNavigationAsset *chosen = before, *created = nullptr;
            uint64_t geometry_key = before ? before->geometry_key : 0;
            bool source_changed = false;
            if (same && before->source_bake && before->key == key && before->revision == (uint32_t)revision) {
                uint64_t current;
                if (!nav_source_key(before, root, &current, state->diagnostic, sizeof(state->diagnostic))) {
                    state->rejected_epoch = root->doc->mutation_epoch;
                    return report();
                }
                source_changed = current != before->geometry_key;
            }
            if (!same || before->key != key || before->revision != (uint32_t)revision || source_changed) {
                void* memory = mem_alloc(sizeof(Scene3dNavigationAsset), MEM_CAT_RENDER);
                if (!memory)
                    return reject("navigation: asset allocation failed");
                created = new (memory) Scene3dNavigationAsset();
                strcpy(created->id, id);
                state->stats.compilations++;
                if (!nav_asset_compile(created, root, node, state->diagnostic, sizeof(state->diagnostic))) {
                    nav_asset_destroy(created);
                    state->rejected_epoch = root->doc->mutation_epoch;
                    return report();
                }
                geometry_key = created->geometry_key;
                bool equivalent = same && before->revision == (uint32_t)revision && nav_asset_equal(before, created);
                bool same_source =
                    same && before->revision == (uint32_t)revision && nav_source_definition_equal(before, created);
                if (same_source) {
                    bool geometry_same = before->geometry_key == geometry_key;
                    if (!geometry_same && before->compiled.bake) {
                        Scene3dNavigationGeometry fresh;
                        if (!scene3d_nav_source_snapshot(created, root, &fresh, nullptr, state->diagnostic,
                                                 sizeof(state->diagnostic))) {
                            nav_asset_destroy(created);
                            state->rejected_epoch = root->doc->mutation_epoch;
                            return report();
                        }
                        geometry_same = !strcmp(fresh.source_hash, before->compiled.source_hash);
                    }
                    equivalent = equivalent && geometry_same;
                    if (!equivalent && geometry_same && before->compiled.bake) {
                        // link-only edits retain the published geometry and replace only connectivity.
                        created->compiled.bake = navigation_bake_retain(before->compiled.bake);
                        created->compiled.graph =
                            const_cast<NavigationGraph*>(navigation_bake_graph(created->compiled.bake));
                        strcpy(created->compiled.source_hash, before->compiled.source_hash);
                        if (!navigation_attach_links(&created->compiled)) {
                            str_copy(state->diagnostic, sizeof(state->diagnostic), created->compiled.diagnostic,
                                     strlen(created->compiled.diagnostic));
                            nav_asset_destroy(created);
                            state->rejected_epoch = root->doc->mutation_epoch;
                            return report();
                        }
                    }
                }
                if (equivalent) {
                    nav_asset_destroy(created);
                    created = nullptr;
                } else {
                    if (next_generation > INT64_MAX) {
                        nav_asset_destroy(created);
                        return reject("navigation: generation quota exceeded");
                    }
                    created->generation = next_generation++;
                    chosen = created;
                }
            }
            nodes += navigation_node_count(chosen->compiled.graph);
            edges += navigation_edge_count(chosen->compiled.graph);
            vertices += navigation_mesh_vertex_count(chosen->compiled.mesh);
            storage_nodes += scene3d_nav_storage_nodes(chosen->compiled);
            auto retained = scene3d_nav_retained_bake(chosen->compiled);
            raw_cells += retained.raw_cells;
            source_triangles += retained.source_triangles;
            if (!scene3d_nav_counts_fit(nodes, edges, vertices, storage_nodes, raw_cells, source_triangles)) {
                nav_asset_destroy(created);
                return reject("navigation: combined scene graph/vertex/storage quota exceeded");
            }
            if (!plans.values.append({chosen, created, ref, key, (uint32_t)revision, geometry_key})) {
                nav_asset_destroy(created);
                return reject("navigation: reconciliation allocation failed");
            }
        }
    }
    // all candidates validate before any old owner is released; future jobs must invalidate at this boundary.
    for (auto* asset = state->assets; asset;) {
        auto* next = asset->next;
        bool kept = false;
        for (const auto& plan : plans.values)
            kept |= plan.chosen == asset;
        if (!kept) {
            for (auto* job = state->jobs; job; job = job->next)
                if (!strcmp(job->asset, asset->id) && job->target_generation == asset->generation)
                    scene3d_nav_job_stale(job, "navigation bake: target or source generation changed");
            state->stats.invalidations++;
            nav_asset_destroy(asset);
        }
        asset = next;
    }
    state->assets = nullptr;
    Scene3dNavigationAsset** tail = &state->assets;
    for (auto& plan : plans.values) {
        auto* asset = plan.chosen;
        asset->source = plan.source;
        asset->key = plan.key;
        asset->revision = plan.revision;
        asset->geometry_key = plan.geometry_key;
        asset->next = nullptr;
        *tail = asset;
        tail = &asset->next;
        plan.created = nullptr;
    }
    state->stats.assets = plans.values.size();
    state->stats.nodes = nodes;
    state->stats.edges = edges;
    state->stats.vertices = vertices;
    state->stats.storage_nodes = storage_nodes;
    state->stats.raw_cells = raw_cells;
    state->stats.source_triangles = source_triangles;
    *state->next_generation = next_generation;
    state->epoch = root->doc->mutation_epoch;
    state->rejected_epoch = UINT64_MAX;
    state->diagnostic[0] = 0;
    return scene3d_nav_agents_schedule(state, root);
}
void scene3d_navigation_follow_rebuild(Scene3dNavigation* state, DomNode* old_root, DomNode* new_root) {
    if (!state)
        return;
    view_state_ref_follow_rebuild(state->document, &state->root, old_root, new_root);
    for (auto* asset = state->assets; asset; asset = asset->next)
        view_state_ref_follow_rebuild(state->document, &asset->source, old_root, new_root);
    for (auto* agent = state->agents; agent; agent = agent->next)
        view_state_ref_follow_rebuild(state->document, &agent->body, old_root, new_root);
    state->epoch = state->rejected_epoch = UINT64_MAX;
}
bool scene3d_navigation_stats(Scene3dNavigation* state, Scene3dNavigationStats* stats) {
    if (!state || !stats)
        return false;
    *stats = state->stats;
    return true;
}
bool scene3d_navigation_generation(Scene3dNavigation* state, const char* id, uint64_t* generation) {
    if (!state || !id || !generation)
        return false;
    for (auto* asset = state->assets; asset; asset = asset->next)
        if (!strcmp(asset->id, id)) {
            *generation = asset->generation;
            return true;
        }
    return false;
}
void scene3d_nav_bake_worker(void* opaque) {
    auto* worker = (Scene3dBakeWorker*)opaque;
    __atomic_store_n(&worker->started, true, __ATOMIC_RELAXED);
    auto proceed = [](void* owner, NavigationBakeStage stage, size_t work) {
        auto* state = (Scene3dBakeWorker*)owner;
        __atomic_store_n(&state->stage, (unsigned)stage, __ATOMIC_RELAXED);
        __atomic_store_n(&state->work, work, __ATOMIC_RELAXED);
        return !__atomic_load_n(&state->cancelled, __ATOMIC_RELAXED);
    };
    NavigationBakeDesc desc = {worker->geometry.triangles.data(), worker->geometry.triangles.size(), worker->settings,
                               proceed, worker};
    NavigationError error = {};
    if (worker->base) {
        NavigationBakeUpdate update = {worker->base, worker->tile_size, worker->max_dirty_tiles};
        worker->success = navigation_bake_update(&desc, &update, &worker->result, &error);
    } else {
        worker->success = navigation_bake_create(&desc, &worker->result, &error);
    }
    if (worker->success)
        navigation_bake_stats(worker->result, &worker->stats);
    else if (error.message)
        str_copy(worker->diagnostic, sizeof(worker->diagnostic), error.message, strlen(error.message));
    // release/acquire publishes native result/error storage; the worker touches nothing afterward.
    __atomic_store_n(&worker->done, true, __ATOMIC_RELEASE);
}
bool scene3d_nav_can_change(Scene3dNavigation* state) {
    Scene3dRuntimeStats clock = {};
    scene3d_runtime_stats(state->runtime, &clock);
    return scene3d_runtime_can_configure(state->runtime, clock.timestep, 1);
}
bool scene3d_navigation_has_agents(Scene3dNavigation* state) {
    return state && state->agents;
}
bool scene3d_nav_agents_schedule(Scene3dNavigation* state, DomElement* root) {
    if (!state->agents)
        return true;
    bool active = false;
    for (auto* agent = state->agents; agent; agent = agent->next)
        active |= agent->target;
    return scene3d_runtime_enable(state->runtime, SCENE3D_TICK_NAVIGATION, state,
                                  active && scene3d_physics_playing(scene3d_physics(root)));
}
Scene3dNavigationAsset* scene3d_nav_agent_asset(Scene3dNavigation* state, const char* id) {
    for (auto* asset = state->assets; asset; asset = asset->next)
        if (!strcmp(asset->id, id))
            return asset;
    return nullptr;
}
void scene3d_nav_agent_stop(Scene3dNavigationAgent* agent, const char* status, const char* diagnostic) {
    agent->status = status;
    agent->pending = false;
    agent->polygon = 0;
    str_copy(agent->diagnostic, sizeof(agent->diagnostic), diagnostic ? diagnostic : "",
             diagnostic ? strlen(diagnostic) : 0);
    navigation_follower_cancel(agent->follower);
    agent->observed = {};
    agent->preferred_velocity = {};
    agent->avoidance_velocity = {};
}
NavigationVec scene3d_nav_agent_foot(const PhysicsCharacterProfile& profile, const PhysicsCharacterMove& move) {
    return {profile.shape_pose.position.x,
            profile.shape_pose.position.y - profile.height * .5 - profile.radius - move.skin,
            profile.shape_pose.position.z};
}
bool scene3d_nav_agent_profile(Scene3dNavigationAsset* asset, const PhysicsCharacterProfile& profile,
                              const PhysicsCharacterMove& move, NavigationSurfaceQuery* query,
                              const char** diagnostic) {
    NavigationBakeSettings settings = {};
    NavigationSurfaceView view = {};
    NavigationError error = {};
    PhysicsVec axis = prot(profile.shape_pose.rotation, {0, 1, 0});
    double radius = profile.radius + move.skin, height = profile.height + 2 * radius;
    if (!asset || !scene3d_nav_asset_ready(asset) || !asset->compiled.bake || asset->compiled.dimensions != 3 ||
        !navigation_bake_settings(asset->compiled.bake, &settings) ||
        !navigation_bake_surface(asset->compiled.bake, &view)) {
        *diagnostic = "navigation agent: requires a published 3D clearance bake";
        return false;
    }
    if (move.up.x != 0 || move.up.y <= 0 || move.up.z != 0 || fabs(axis.y) < 1 - 1e-8 ||
        settings.max_slope > move.slope_angle) {
        *diagnostic = "navigation agent: requires an upright Y-up capsule and compatible slope limit";
        return false;
    }
    // script options cannot understate the actual controller's footprint or step requirement.
    query->radius = radius;
    query->height = height;
    query->max_step = move.step_height;
    if (!navigation_surface_profile_valid(&view, query, &error)) {
        *diagnostic = error.message;
        return false;
    }
    return true;
}
static bool nav_agent_event(Scene3dNavigation* state, DomElement* root, Scene3dNavigationAgent* agent,
                            const char* type) {
    return scene3d_navigation_event_publish(root, state, agent, type);
}
bool scene3d_nav_agent_observe(Scene3dNavigation* state, DomElement* root, Scene3dNavigationAgent* agent) {
    auto* physics = scene3d_physics(root);
    auto* body = dom_node_ref_validate(state->document, agent->body);
    uint64_t identity = body && body->is_element() ? scene3d_physics_character_body(physics, body->as_element()) : 0;
    PhysicsCharacterProfile profile = {};
    PhysicsError error = {};
    if (!identity || !scene3d_physics_character_profile(physics, identity, &profile, &error)) {
        scene3d_nav_agent_stop(agent, "invalid",
                       error.message ? error.message : "navigation agent: body detached from this world");
        return false;
    }
    agent->movement.body = identity;
    agent->foot = scene3d_nav_agent_foot(profile, agent->movement);
    agent->precision = profile.precision;
    auto* asset = scene3d_nav_agent_asset(state, agent->asset);
    const char* diagnostic = nullptr;
    if (!scene3d_nav_agent_profile(asset, profile, agent->movement, &agent->query, &diagnostic)) {
        scene3d_nav_agent_stop(agent, "invalid", diagnostic);
        return false;
    }
    uint64_t world = scene3d_physics_world_identity(physics);
    if (world != agent->world || asset->generation != agent->asset_generation) {
        scene3d_nav_agent_stop(agent, agent->target ? "queued" : "idle");
        agent->pending = agent->target;
        agent->world = world;
        agent->asset_generation = asset->generation;
        agent->movement.platform = 0;
    }
    return true;
}
bool scene3d_nav_agent_support(Scene3dNavigationAsset* asset, Scene3dNavigationAgent* agent, uint64_t polygon,
                              NavigationVec foot, NavigationVec* position) {
    NavigationBakeSupport support = {};
    NavigationBakeSettings settings = {};
    if (!navigation_bake_support(asset->compiled.bake, polygon, foot, &support) ||
        !navigation_bake_settings(asset->compiled.bake, &settings))
        return false;
    double tolerance = 4 * agent->precision,
           above = settings.max_step + agent->query.radius * (1 / cos(settings.max_slope) - 1);
    // raster span bounds support below the flattened floor; steps and normal capsule clearance bound it above.
    if (hypot(foot.x - support.position.x, foot.z - support.position.z) > tolerance ||
        foot.y < support.low - tolerance || foot.y > support.position.y + above + tolerance ||
        foot.y + agent->query.height > support.ceiling + tolerance)
        return false;
    *position = support.position;
    return true;
}
static bool nav_agent_search(Scene3dNavigation* state, Scene3dNavigationAgent* agent) {
    auto* asset = scene3d_nav_agent_asset(state, agent->asset);
    NavigationError error = {};
    struct Route {
        NavigationMeshPath value = {};
        ~Route() {
            navigation_mesh_path_clear(&value);
        }
    } route;
    agent->queries++;
    state->stats.queries++;
    agent->pending = false;
    NavigationSurfaceView surface = {};
    navigation_bake_surface(asset->compiled.bake, &surface);
    size_t tests = 0;
    uint64_t start = 0;
    double best = INFINITY;
    for (uint64_t polygon = 1; polygon <= surface.polygon_count; polygon++) {
        if (tests == agent->query.path.max_projection_tests) {
            scene3d_nav_agent_stop(agent, "budget_exhausted", "navigation agent: support projection budget exhausted");
            return false;
        }
        tests++;
        NavigationVec position;
        if (!navigation_node_admitted(surface.graph, polygon, &agent->query.path.search) ||
            !scene3d_nav_agent_support(asset, agent, polygon, agent->foot, &position))
            continue;
        double distance = fabs(position.y - agent->foot.y);
        if (distance < best) {
            best = distance;
            start = polygon;
            agent->position = position;
        }
    }
    if (!start) {
        scene3d_nav_agent_stop(agent, "off_surface", "navigation agent: achieved foot has no compatible baked support layer");
        return false;
    }
    auto query = agent->query;
    query.path.start = agent->position;
    query.path.goal = agent->goal;
    query.path.max_projection_tests -= tests;
    if (!navigation_surface_find_compiled(asset->compiled, &query, &route.value, &error)) {
        scene3d_nav_agent_stop(agent, "blocked", error.message);
        return false;
    }
    if (!route.value.point_count) {
        scene3d_nav_agent_stop(agent, route.value.corridor.status == NAVIGATION_EXHAUSTED ? "budget_exhausted" : "unreachable");
        return false;
    }
    if (route.value.point_count > NAV_AGENT_ROUTE_LIMIT || route.value.traversal_count > 128) {
        scene3d_nav_agent_stop(agent, "budget_exhausted", "navigation agent: copied route quota exceeded");
        return false;
    }
    if (*state->next_generation > INT64_MAX) {
        scene3d_nav_agent_stop(agent, "blocked", "navigation agent: path generation quota exceeded");
        return false;
    }
    if (!navigation_follower_assign(agent->follower, &route.value, *state->next_generation, &error)) {
        scene3d_nav_agent_stop(agent, "blocked", error.message);
        return false;
    }
    agent->path = (*state->next_generation)++;
    agent->polygon = route.value.start_polygon;
    agent->status = "moving";
    agent->diagnostic[0] = 0;
    navigation_follower_read(agent->follower, &agent->observed);
    return true;
}
static bool nav_agent_walk(Scene3dNavigationAsset* asset, Scene3dNavigationAgent* agent, NavigationVec end,
                           NavigationSurfaceMotion* motion) {
    NavigationSurfaceView surface = {};
    NavigationError error = {};
    navigation_bake_surface(asset->compiled.bake, &surface);
    NavigationSurfaceMove movement = {agent->query, agent->polygon, 32, 256};
    movement.profile.path.start = agent->position;
    movement.profile.path.goal = end;
    if (!navigation_surface_walk(&surface, &movement, motion, &error)) {
        scene3d_nav_agent_stop(agent, "blocked", error.message);
        return false;
    }
    if (!motion->polygon) {
        scene3d_nav_agent_stop(agent, motion->status == NAVIGATION_EXHAUSTED ? "budget_exhausted" : "off_surface",
                       "navigation agent: no verified starting polygon");
        return false;
    }
    // projection may select a route endpoint, but it must never pull the body onto a disconnected floor.
    if (motion->start_distance > 4 * agent->precision) {
        scene3d_nav_agent_stop(agent, "off_surface", "navigation agent: retained navigation position left its baked domain");
        return false;
    }
    return true;
}
static bool nav_agent_ground(Scene3dNavigationAsset* asset, Scene3dNavigationAgent* agent,
                             const PhysicsCharacterState& achieved) {
    if (!achieved.grounded) {
        scene3d_nav_agent_stop(agent, "blocked",
                       "navigation agent: controller target has no ground support within snap distance");
        return false;
    }
    NavigationBakeSettings settings = {};
    navigation_bake_settings(asset->compiled.bake, &settings);
    if (achieved.ground_normal.y + 8 * DBL_EPSILON < cos(settings.max_slope)) {
        scene3d_nav_agent_stop(agent, "blocked", "navigation agent: physical support exceeds the baked slope limit");
        return false;
    }
    return true;
}
struct NavigationAgentFrame {
    PhysicsPose pose = {}, platform_pose = {};
    NavigationVec foot = {}, velocity = {};
    uint64_t polygon = 0, platform = 0;
    bool target = false;
    bool steer = false;
};
static void nav_agent_propose(Scene3dNavigationAsset* asset, Scene3dNavigationAgent* agent,
                              const PhysicsCharacterProfile& before, const PhysicsCharacterState& achieved,
                              double delta, NavigationAgentFrame* frame) {
    if (!nav_agent_ground(asset, agent, achieved))
        return;
    // collision recovery and sliding can change direction; validate the computed target before committing it.
    PhysicsPose shape = pcompose(achieved.pose, pcompose(ppose_inverse(before.pose), before.shape_pose));
    PhysicsCharacterProfile proposed = before;
    proposed.shape_pose = shape;
    NavigationVec final = scene3d_nav_agent_foot(proposed, agent->movement), position;
    NavigationSurfaceMotion verified = {};
    if (!nav_agent_walk(asset, agent, final, &verified))
        return;
    if (!scene3d_nav_agent_support(asset, agent, verified.polygon, final, &position) ||
        hypot(verified.end.x - final.x, verified.end.z - final.z) > 4 * agent->precision) {
        scene3d_nav_agent_stop(agent, "blocked", "navigation agent: controller target leaves the baked domain");
        return;
    }
    frame->pose = achieved.pose;
    frame->foot = final;
    frame->velocity = {(final.x - agent->foot.x) / delta, (final.y - agent->foot.y) / delta,
                       (final.z - agent->foot.z) / delta};
    frame->polygon = verified.polygon;
    frame->platform = achieved.platform;
    frame->platform_pose = achieved.platform_pose;
    frame->target = true;
}
static bool nav_agents_publish(Scene3dNavigationAgent* const* agents, const NavigationAgentFrame* frames, size_t count,
                               Scene3dPhysics* physics) {
    PhysicsCommand commands[NAV_AGENT_LIMIT] = {};
    size_t command_count = 0;
    for (size_t i = 0; i < count; i++) {
        if (!frames[i].target)
            continue;
        auto& command = commands[command_count++];
        command.kind = PHYSICS_TARGET;
        command.body = agents[i]->movement.body;
        command.pose = frames[i].pose;
    }
    if (!command_count)
        return true;
    PhysicsError error = {};
    // validate and publish all verified targets together before updating retained controller feedback.
    if (!scene3d_physics_apply(physics, commands, command_count, &error)) {
        for (size_t i = 0; i < count; i++) {
            if (frames[i].target)
                scene3d_nav_agent_stop(agents[i], "blocked", error.message);
        }
        return false;
    }
    for (size_t i = 0; i < count; i++) {
        if (!frames[i].target)
            continue;
        agents[i]->polygon = frames[i].polygon;
        agents[i]->movement.platform = frames[i].platform;
        agents[i]->movement.platform_pose = frames[i].platform_pose;
        agents[i]->achieved_velocity = frames[i].velocity;
    }
    return true;
}
static void nav_agent_prepare(Scene3dNavigation* state, DomElement* root, Scene3dNavigationAgent* agent, double delta,
                              NavigationAvoidanceAgent* crowd, NavigationAgentFrame* frame) {
    if (agent->observed.status != NAVIGATION_FOLLOW_MOVING)
        return;
    auto* asset = scene3d_nav_agent_asset(state, agent->asset);
    if (!scene3d_nav_agent_support(asset, agent, agent->polygon, agent->foot, &agent->position)) {
        scene3d_nav_agent_stop(agent, "off_surface", "navigation agent: achieved foot left its retained support layer");
        return;
    }
    PhysicsCharacterMove movement = agent->movement;
    movement.displacement = {};
    PhysicsCharacterProfile before = {};
    PhysicsCharacterState achieved = {};
    PhysicsError physics_error = {};
    auto* physics = scene3d_physics(root);
    if (!scene3d_physics_character_profile(physics, movement.body, &before, &physics_error) ||
        !scene3d_physics_character_compute(physics, &movement, &achieved, &physics_error)) {
        scene3d_nav_agent_stop(agent, "blocked", physics_error.message);
        return;
    }
    // settle through the controller before claiming progress/arrival; a projected or airborne foot is not achieved
    // support.
    if (!nav_agent_ground(asset, agent, achieved))
        return;
    if (plen(achieved.achieved) > 4 * agent->precision) {
        nav_agent_propose(asset, agent, before, achieved, delta, frame);
        crowd->preferred = {frame->velocity.x, 0, frame->velocity.z};
        return;
    }
    NavigationError error = {};
    if (!navigation_follower_feedback(agent->follower, agent->achieved_velocity, &error) ||
        !navigation_follower_step(agent->follower, agent->position, agent->path, delta, &agent->observed, &error)) {
        scene3d_nav_agent_stop(agent, "blocked", error.message);
        return;
    }
    static const char* statuses[] = {"idle", "moving", "action", "arrived", "path_end", "stale", "off_path", "stuck"};
    agent->status = statuses[agent->observed.status];
    if (agent->observed.status != NAVIGATION_FOLLOW_MOVING)
        return;
    crowd->preferred = agent->observed.velocity;
    crowd->responsive = agent->avoidance.enabled;
    // a steering plan precedes the controller target; settling may already have produced a target instead.
    frame->steer = true;
}
static void nav_agent_execute(Scene3dNavigation* state, DomElement* root, Scene3dNavigationAgent* agent,
                              NavigationVec velocity, double delta, NavigationAgentFrame* frame) {
    if (agent->observed.status != NAVIGATION_FOLLOW_MOVING)
        return;
    auto* asset = scene3d_nav_agent_asset(state, agent->asset);
    NavigationVec intended = {agent->position.x + velocity.x * delta, agent->position.y,
                              agent->position.z + velocity.z * delta};
    NavigationSurfaceMotion safe = {};
    if (!nav_agent_walk(asset, agent, intended, &safe))
        return;
    PhysicsCharacterMove movement = agent->movement;
    movement.displacement = {safe.end.x - agent->foot.x, 0, safe.end.z - agent->foot.z};
    PhysicsCharacterProfile before = {};
    PhysicsCharacterState achieved = {};
    PhysicsError error = {};
    auto* physics = scene3d_physics(root);
    if (!scene3d_physics_character_profile(physics, movement.body, &before, &error) ||
        !scene3d_physics_character_compute(physics, &movement, &achieved, &error)) {
        scene3d_nav_agent_stop(agent, "blocked", error.message);
        return;
    }
    nav_agent_propose(scene3d_nav_agent_asset(state, agent->asset), agent, before, achieved, delta, frame);
}
Scene3dTickResult scene3d_nav_agents_tick(void* owner, double delta) {
    auto* state = (Scene3dNavigation*)owner;
    auto* node = dom_node_ref_validate(state->document, state->root);
    if (!node || !node->is_element())
        return SCENE3D_TICK_IDLE;
    auto* root = node->as_element();
    Scene3dNavigationAgent* agents[NAV_AGENT_LIMIT];
    size_t count = 0;
    for (auto* agent = state->agents; agent; agent = agent->next)
        agents[count++] = agent;
    if (!count)
        return SCENE3D_TICK_IDLE;
    bool readable[NAV_AGENT_LIMIT] = {};
    const char* previous[NAV_AGENT_LIMIT] = {};
    for (size_t i = 0; i < count; i++) {
        auto* agent = agents[i];
        previous[i] = agent->status;
        NavigationVec old_foot = agent->foot;
        uint64_t old_world = agent->world;
        readable[i] = scene3d_nav_agent_observe(state, root, agent);
        agent->achieved_velocity = {};
        agent->preferred_velocity = {};
        agent->avoidance_velocity = {};
        if (readable[i] && old_world == agent->world) {
            agent->achieved_velocity = {(agent->foot.x - old_foot.x) / delta, 0, (agent->foot.z - old_foot.z) / delta};
            if (!navigation_vec_finite(agent->achieved_velocity))
                agent->achieved_velocity = {};
        }
    }
    size_t searched = 0, start = state->next_agent % count;
    for (size_t turn = 0; turn < count; turn++) {
        size_t i = (start + turn) % count;
        auto* agent = agents[i];
        if (!readable[i] || !agent->pending)
            continue;
        if (searched == NAV_AGENT_SEARCHES)
            break;
        searched++;
        state->next_agent = (i + 1) % count;
        if (nav_agent_search(state, agent) && !nav_agent_event(state, root, agent, "navigationpathchanged"))
            return SCENE3D_TICK_FAILED;
    }
    NavigationAvoidanceAgent crowd[NAV_AGENT_LIMIT] = {};
    NavigationAgentFrame frames[NAV_AGENT_LIMIT] = {};
    size_t crowd_slots[NAV_AGENT_LIMIT];
    for (size_t i = 0; i < count; i++)
        crowd_slots[i] = SIZE_MAX;
    size_t crowd_count = 0;
    for (size_t i = 0; i < count; i++)
        if (readable[i]) {
            auto* agent = agents[i];
            auto& member = crowd[crowd_count];
            crowd_slots[i] = crowd_count++;
            member.id = agent->movement.body;
            member.position = agent->foot;
            member.preferred = {};
            member.radius = agent->query.radius;
            member.height = agent->query.height;
            member.priority = agent->avoidance.priority;
            member.time_horizon = agent->avoidance.time_horizon;
            member.max_neighbors = agent->avoidance.max_neighbors;
            member.max_neighbor_tests = agent->avoidance.max_neighbor_tests;
            member.max_solver_tests = agent->avoidance.max_solver_tests;
            member.responsive = false;
            if (agent->target && !agent->pending && agent->observed.status != NAVIGATION_FOLLOW_IDLE)
                nav_agent_prepare(state, root, agent, delta, &member, &frames[i]);
            member.max_speed = fmax(agent->avoidance.max_speed, hypot(member.preferred.x, member.preferred.z));
            agent->preferred_velocity = member.preferred;
        }
    NavigationAvoidanceResult choices[NAV_AGENT_LIMIT] = {};
    NavigationError avoidance_error = {};
    if (!navigation_avoidance_step(crowd, crowd_count, delta, choices, &avoidance_error)) {
        for (size_t i = 0; i < count; i++) {
            if (frames[i].steer || frames[i].target) {
                scene3d_nav_agent_stop(agents[i], "blocked", avoidance_error.message);
                agents[i]->avoidance_status = NAVIGATION_AVOIDANCE_BLOCKED;
                frames[i] = {};
            }
        }
    } else {
        for (size_t i = 0; i < count; i++)
            if (frames[i].steer || frames[i].target) {
                auto* agent = agents[i];
                auto& choice = choices[crowd_slots[i]];
                // stationary participants constrain neighbors but retain their last completed solve diagnostics.
                agent->avoidance_neighbors = choice.neighbors;
                agent->avoidance_neighbor_tests = choice.neighbor_tests;
                agent->avoidance_solver_tests = choice.solver_tests;
                agent->avoidance_status = choice.status;
                if (choice.status == NAVIGATION_AVOIDANCE_BLOCKED || choice.status == NAVIGATION_AVOIDANCE_EXHAUSTED) {
                    const char* status =
                        choice.status == NAVIGATION_AVOIDANCE_EXHAUSTED ? "budget_exhausted" : "blocked";
                    const char* diagnostic = choice.status == NAVIGATION_AVOIDANCE_EXHAUSTED
                                                 ? "navigation agent: avoidance work budget exhausted"
                                                 : "navigation agent: local avoidance constraints are infeasible";
                    scene3d_nav_agent_stop(agent, status, diagnostic);
                    frames[i] = {};
                }
            }
    }
    NavigationVec actual[NAV_AGENT_LIMIT] = {};
    for (size_t i = 0; i < count; i++)
        if (crowd_slots[i] != SIZE_MAX) {
            auto* agent = agents[i];
            size_t slot = crowd_slots[i];
            if (frames[i].steer) {
                NavigationVec velocity = crowd[slot].responsive ? choices[slot].velocity : crowd[slot].preferred;
                agent->avoidance_velocity = velocity;
                agent->preferred_velocity = crowd[slot].preferred;
                agent->observed.velocity = velocity;
                nav_agent_execute(state, root, agent, velocity, delta, &frames[i]);
            }
            if (frames[i].target)
                actual[slot] = frames[i].velocity;
        }
    bool clear = false;
    if (!navigation_avoidance_verify(crowd, actual, crowd_count, delta, &clear, &avoidance_error) || !clear) {
        for (size_t i = 0; i < count; i++)
            if (crowd_slots[i] != SIZE_MAX && frames[i].target) {
                auto* agent = agents[i];
                scene3d_nav_agent_stop(agent, "blocked",
                               "navigation agent: simultaneous controller targets enter a shared capsule");
                agent->avoidance_status = NAVIGATION_AVOIDANCE_BLOCKED;
                frames[i].target = false;
            }
    }
    bool published = nav_agents_publish(agents, frames, count, scene3d_physics(root));
    for (size_t i = 0; i < count; i++) {
        auto* agent = agents[i];
        if (strcmp(previous[i], agent->status)) {
            const char* type = !strcmp(agent->status, "action")    ? "navigationlinkrequired"
                               : !strcmp(agent->status, "arrived") ? "navigationarrived"
                                                                   : "navigationstatechanged";
            if (!nav_agent_event(state, root, agent, type))
                return SCENE3D_TICK_FAILED;
        }
    }
    return published ? SCENE3D_TICK_ACTIVE : SCENE3D_TICK_FAILED;
}
