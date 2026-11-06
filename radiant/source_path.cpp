#include "source_path.hpp"
#include "../lib/memtrack.h"
#include <string.h>

extern "C" {

void source_path_init(SourcePathC* p) {
    if (!p) return;
    p->indices = NULL;
    p->depth = 0;
}

void source_path_free(SourcePathC* p) {
    if (!p) return;
    mem_free(p->indices);
    p->indices = NULL;
    p->depth = 0;
}

SourcePathC source_path_clone(const SourcePathC* p) {
    SourcePathC out = { NULL, 0 };
    if (!p || p->depth == 0 || !p->indices) return out;
    out.indices = (int*)mem_alloc(sizeof(int) * (size_t)p->depth, MEM_CAT_RENDER);
    if (!out.indices) return out;
    memcpy(out.indices, p->indices, sizeof(int) * (size_t)p->depth);
    out.depth = p->depth;
    return out;
}

bool source_path_equal(const SourcePathC* a, const SourcePathC* b) {
    if (!a || !b) return false;
    if (a->depth != b->depth) return false;
    if (a->depth == 0) return true;
    return memcmp(a->indices, b->indices, sizeof(int) * (size_t)a->depth) == 0;
}

void source_pos_free(SourcePosC* p) {
    if (!p) return;
    source_path_free(&p->path);
    p->offset = 0;
    p->kind = SOURCE_POS_TEXT;
}

}
