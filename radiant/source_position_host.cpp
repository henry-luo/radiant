#include "source_position_host.hpp"
#include "../lib/provider.hpp"

static const RadiantSourcePositionHostOps* source_position_host_ops = nullptr;

bool radiant_source_position_host_register(const RadiantSourcePositionHostOps* operations) {
    return lambda_provider_register(&source_position_host_ops, operations,
        &RadiantSourcePositionHostOps::record, &RadiantSourcePositionHostOps::reset,
        &RadiantSourcePositionHostOps::reverse_lookup, &RadiantSourcePositionHostOps::position_lookup);
}

extern "C" void render_map_record_path(Item source, const char* template_ref, const int* indices, int depth) {
    const RadiantSourcePositionHostOps* operations = lambda_provider_load(&source_position_host_ops);
    if (operations) operations->record(source, template_ref, indices, depth);
}

extern "C" void source_pos_bridge_reset(void) {
    const RadiantSourcePositionHostOps* operations = lambda_provider_load(&source_position_host_ops);
    if (operations) operations->reset();
}

extern "C" bool render_map_reverse_lookup_with_path(Item result, RenderMapLookup* lookup, SourcePathC* path) {
    const RadiantSourcePositionHostOps* operations = lambda_provider_load(&source_position_host_ops);
    if (operations) return operations->reverse_lookup(result, lookup, path);
    if (path) source_path_init(path);
    return false;
}

bool radiant_source_position_lookup(Item result, Item* source, SourcePathC* path) {
    const RadiantSourcePositionHostOps* operations = lambda_provider_load(&source_position_host_ops);
    return operations && operations->position_lookup(result, source, path);
}
