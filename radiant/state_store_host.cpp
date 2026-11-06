#include "state_store_host.hpp"
#include "../lib/provider.hpp"
#include "../lib/log.h"
#include <stdlib.h>

static const RadiantStateStoreHostOps* state_store_host_ops = nullptr;

static const RadiantStateStoreHostOps* required_state_store_host() {
    const RadiantStateStoreHostOps* operations = lambda_provider_load(&state_store_host_ops);
    if (!operations) {
        // Native-only documents never own runtime maps or registered roots.
        log_error("state_store_host: runtime-owned state has no host");
        abort();
    }
    return operations;
}

bool radiant_state_store_host_register(const RadiantStateStoreHostOps* operations) {
    return lambda_provider_register(&state_store_host_ops, operations,
        &RadiantStateStoreHostOps::initialize, &RadiantStateStoreHostOps::enter,
        &RadiantStateStoreHostOps::current, &RadiantStateStoreHostOps::release_root,
        &RadiantStateStoreHostOps::register_current_root, &RadiantStateStoreHostOps::release_current_root,
        &RadiantStateStoreHostOps::detach_maps);
}

bool radiant_state_store_host_initialize(DomDocument* document, DocState* state, EvalContext** owner) {
    const RadiantStateStoreHostOps* operations = lambda_provider_load(&state_store_host_ops);
    *owner = nullptr;
    return !operations || operations->initialize(document, state, owner);
}

bool radiant_state_store_host_enter(EvalContext* owner) {
    const RadiantStateStoreHostOps* operations = lambda_provider_load(&state_store_host_ops);
    return !owner || (operations && operations->enter(owner));
}

EvalContext* radiant_state_store_host_current() {
    const RadiantStateStoreHostOps* operations = lambda_provider_load(&state_store_host_ops);
    return operations ? operations->current() : nullptr;
}

void radiant_state_store_host_release_root(EvalContext* owner, uint64_t* root) {
    required_state_store_host()->release_root(owner, root);
}

bool radiant_state_store_host_register_current_root(uint64_t* root) {
    const RadiantStateStoreHostOps* operations = lambda_provider_load(&state_store_host_ops);
    return operations && operations->register_current_root(root);
}

void radiant_state_store_host_release_current_root(uint64_t* root) {
    required_state_store_host()->release_current_root(root);
}

void radiant_state_store_host_detach_maps(DocState* state) {
    required_state_store_host()->detach_maps(state);
}
