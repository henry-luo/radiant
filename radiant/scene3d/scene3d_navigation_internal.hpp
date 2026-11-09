#pragma once

#include "scene3d.hpp"
#include "scene3d_source.hpp"
#include "lambda/core/navigation_graph.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"
#include "lib/thread_pool.h"

// Native navigation state and worker ownership shared with the Lambda adapter.
struct Scene3dNavigationSource {
    char id[128] = {};
    bool walkable = true;
    uint32_t groups = UINT32_MAX;
    double cost = 1;
};
// this is the entire worker boundary: no DOM refs, Item values or script callbacks.
struct Scene3dBakeWorker {
    Scene3dNavigationGeometry geometry;
    NavigationBakeSettings settings = {};
    NavigationBake* result = nullptr;
    NavigationBake* base = nullptr;
    size_t tile_size = 0, max_dirty_tiles = 0;
    NavigationBakeStats stats = {};
    bool cancelled = false, started = false, done = false, success = false;
    unsigned stage = 0;
    size_t work = 0;
    char diagnostic[256] = {};
    ~Scene3dBakeWorker() {
        navigation_bake_destroy(result);
        navigation_bake_destroy(base);
    }
};
struct Scene3dBakeJob {
    Scene3dBakeJob* next = nullptr;
    Scene3dBakeWorker worker;
    DomNodeRef root = {}, asset_ref = {};
    lam::ArrayList<DomNodeRef> references;
    NavigationCompiled definitions;
    char asset[128] = {}, settings_hash[65] = {}, diagnostic[256] = {};
    uint64_t token = 0, target_generation = 0, published_generation = 0;
    size_t charged_triangles = 0;
    bool stale = false;
};

struct Scene3dNavigationAsset {
    Scene3dNavigationAsset* next = nullptr;
    DomNodeRef source = {};
    char id[128] = {};
    uint64_t key = 0, generation = 0;
    uint32_t revision = 0;
    NavigationCompiled compiled;
    bool source_bake = false;
    uint64_t geometry_key = 0, latest_job = 0;
    NavigationBakeSettings settings = {};
    lam::ArrayList<Scene3dNavigationSource> sources;
};
static constexpr size_t NAV_AGENT_LIMIT = NAVIGATION_AVOIDANCE_MAX_AGENTS, NAV_AGENT_SEARCHES = 4,
                        NAV_AGENT_ROUTE_LIMIT = 4096;
struct NavigationAgentAvoidance {
    bool enabled = true;
    double priority = .5, time_horizon = 2, max_speed = 2;
    size_t max_neighbors = NAVIGATION_AVOIDANCE_MAX_AGENTS - 1;
    size_t max_neighbor_tests = NAVIGATION_AVOIDANCE_MAX_AGENTS - 1, max_solver_tests = 1024;
};
struct Scene3dNavigationAgent {
    Scene3dNavigationAgent* next = nullptr;
    DomNodeRef body = {};
    char id[128] = {}, asset[128] = {}, diagnostic[256] = {};
    uint64_t command = 0, path = 0, asset_generation = 0, world = 0, polygon = 0, queries = 0;
    PhysicsCharacterMove movement = {};
    NavigationSurfaceQuery query = {};
    NavigationVec goal = {}, foot = {}, position = {};
    NavigationFollower* follower = nullptr;
    NavigationVec preferred_velocity = {}, achieved_velocity = {};
    NavigationAgentAvoidance avoidance = {};
    NavigationVec avoidance_velocity = {};
    NavigationAvoidanceStatus avoidance_status = NAVIGATION_AVOIDANCE_DISABLED;
    size_t avoidance_neighbors = 0, avoidance_neighbor_tests = 0, avoidance_solver_tests = 0;
    NavigationFollowState observed = {};
    const char* status = "idle";
    bool target = false, pending = false;
    double precision = 0;
    ~Scene3dNavigationAgent() {
        navigation_follower_destroy(follower);
    }
};
struct Scene3dNavigation {
    DomDocument* document = nullptr;
    DomNodeRef root = {};
    Scene3dRuntime* runtime = nullptr;
    Scene3dNavigationAsset* assets = nullptr;
    Scene3dNavigationStats stats = {};
    uint64_t epoch = UINT64_MAX, rejected_epoch = UINT64_MAX;
    uint64_t* next_generation = nullptr;
    char diagnostic[256] = {};
    ThreadPool* pool = nullptr;
    Scene3dBakeJob* jobs = nullptr;
    Scene3dNavigationAgent* agents = nullptr;
    size_t agent_count = 0, next_agent = 0;
};
Scene3dTickResult scene3d_nav_agents_tick(void* owner, double delta);
bool scene3d_nav_agents_schedule(Scene3dNavigation* state, DomElement* root);
void scene3d_nav_agent_destroy(Scene3dNavigationAgent* agent);
void scene3d_nav_job_destroy(Scene3dBakeJob* job);
void scene3d_nav_job_stale(Scene3dBakeJob* job, const char* message);
bool scene3d_nav_source_snapshot(const Scene3dNavigationAsset* asset, DomElement* root,
                                 Scene3dNavigationGeometry* geometry, lam::ArrayList<DomNodeRef>* refs,
                                 char* diagnostic, size_t capacity);
bool scene3d_nav_asset_ready(const Scene3dNavigationAsset* asset);
size_t scene3d_nav_storage_nodes(const NavigationCompiled& compiled);
NavigationBakeStats scene3d_nav_retained_bake(const NavigationCompiled& compiled);
bool scene3d_nav_counts_fit(size_t nodes, size_t edges, size_t vertices, size_t storage_nodes,
                            size_t raw_cells, size_t source_triangles);
void scene3d_nav_bake_worker(void* opaque);
bool scene3d_nav_can_change(Scene3dNavigation* state);
Scene3dNavigationAsset* scene3d_nav_agent_asset(Scene3dNavigation* state, const char* id);
void scene3d_nav_agent_stop(Scene3dNavigationAgent* agent, const char* status, const char* diagnostic = nullptr);
NavigationVec scene3d_nav_agent_foot(const PhysicsCharacterProfile& profile, const PhysicsCharacterMove& move);
bool scene3d_nav_agent_profile(Scene3dNavigationAsset* asset, const PhysicsCharacterProfile& profile,
                               const PhysicsCharacterMove& move, NavigationSurfaceQuery* query, const char** diagnostic);
bool scene3d_nav_agent_observe(Scene3dNavigation* state, DomElement* root, Scene3dNavigationAgent* agent);
bool scene3d_nav_agent_support(Scene3dNavigationAsset* asset, Scene3dNavigationAgent* agent, uint64_t polygon,
                               NavigationVec foot, NavigationVec* position);
