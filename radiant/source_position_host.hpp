#pragma once

#include "source_path.hpp"
#include "lambda/core/data.hpp"

struct RenderMapLookup;

// Lookup outputs are borrowed Items and owned path copies, valid for the caller's turn.
struct RadiantSourcePositionHostOps {
    void (*record)(Item source, const char* template_ref, const int* indices, int depth);
    void (*reset)();
    bool (*reverse_lookup)(Item result, RenderMapLookup* lookup, SourcePathC* path);
    bool (*position_lookup)(Item result, Item* source, SourcePathC* path);
};
bool radiant_source_position_host_register(const RadiantSourcePositionHostOps* operations);
bool radiant_source_position_lookup(Item result, Item* source, SourcePathC* path);

extern "C" void render_map_record_path(Item source, const char* template_ref, const int* indices, int depth);
extern "C" void source_pos_bridge_reset(void);
extern "C" bool render_map_reverse_lookup_with_path(Item result, RenderMapLookup* lookup, SourcePathC* path);
