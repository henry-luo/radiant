#pragma once

#include <stdint.h>

struct DomDocument;
struct DocState;
struct EvalContext;

// Runtime-owned maps and roots are optional for a native-only document.
struct RadiantStateStoreHostOps {
    bool (*initialize)(DomDocument* document, DocState* state, EvalContext** owner);
    bool (*enter)(EvalContext* owner);
    EvalContext* (*current)();
    void (*release_root)(EvalContext* owner, uint64_t* root);
    bool (*register_current_root)(uint64_t* root);
    void (*release_current_root)(uint64_t* root);
    void (*detach_maps)(DocState* state);
};

bool radiant_state_store_host_register(const RadiantStateStoreHostOps* operations);
bool radiant_state_store_host_initialize(DomDocument* document, DocState* state, EvalContext** owner);
bool radiant_state_store_host_enter(EvalContext* owner);
EvalContext* radiant_state_store_host_current();
void radiant_state_store_host_release_root(EvalContext* owner, uint64_t* root);
bool radiant_state_store_host_register_current_root(uint64_t* root);
void radiant_state_store_host_release_current_root(uint64_t* root);
void radiant_state_store_host_detach_maps(DocState* state);
