#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// A source path owns its child-index array; callers release it with source_path_free.
typedef struct SourcePathC {
    int* indices;
    int depth;
} SourcePathC;

typedef enum SourcePosKind {
    SOURCE_POS_TEXT = 0,    // offset is a code-point offset within text
    SOURCE_POS_ELEMENT = 1, // offset is a child index within a container
} SourcePosKind;

typedef struct SourcePosC {
    SourcePathC path;
    uint32_t offset;
    SourcePosKind kind;
} SourcePosC;

void source_path_init(SourcePathC* path);
void source_path_free(SourcePathC* path);
SourcePathC source_path_clone(const SourcePathC* path);
bool source_path_equal(const SourcePathC* a, const SourcePathC* b);
void source_pos_free(SourcePosC* position);

#ifdef __cplusplus
}
#endif
