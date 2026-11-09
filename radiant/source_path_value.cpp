#include "source_path_value.hpp"
#include "lambda/io/mark_builder.hpp"
#include "lambda/core/mark_reader.hpp"
#include "lambda/core/lambda-decimal.hpp"
#include "lib/memtrack.h"
#include <limits.h>
#include <math.h>

// ---------------------------------------------------------------------------
// MarkBuilder helpers (C++ only). Build the Lambda `pos` and `selection`
// shapes used by lambda/editor/mod_source_pos.ls:
//   pos       = { path: [int, ...], offset: int }
//   selection = { kind: 'text', anchor: pos, head: pos }   (text)
//             | { kind: 'node', path: [int, ...] }         (node)
// ---------------------------------------------------------------------------

static Item path_to_item(MarkBuilder& mb, const SourcePathC* p) {
    ArrayBuilder arr = mb.array();
    if (p && p->indices) {
        for (int i = 0; i < p->depth; i++) {
            arr.append((int64_t)p->indices[i]);
        }
    }
    return arr.final();
}

Item source_pos_to_item(MarkBuilder& mb, const SourcePosC* pos) {
    MapBuilder m = mb.map();
    m.put("path", path_to_item(mb, pos ? &pos->path : nullptr));
    m.put("offset", (int64_t)(pos ? pos->offset : 0));
    return m.final();
}

Item source_text_selection_to_item(MarkBuilder& mb,
                                   const SourcePosC* anchor,
                                   const SourcePosC* head) {
    MapBuilder m = mb.map();
    m.put("kind",   mb.createSymbolItem("text"));
    m.put("anchor", source_pos_to_item(mb, anchor));
    m.put("head",   source_pos_to_item(mb, head));
    return m.final();
}

Item source_node_selection_to_item(MarkBuilder& mb, const SourcePathC* path) {
    MapBuilder m = mb.map();
    m.put("kind", mb.createSymbolItem("node"));
    m.put("path", path_to_item(mb, path));
    return m.final();
}

// ---------------------------------------------------------------------------
// Decode source positions independently of DOM and render-map lookup.
// ---------------------------------------------------------------------------

namespace {

// Source positions cross the Lambda/Radiant ABI as ordinary Lambda numbers.
// Editor arithmetic promotes coordinates to Decimal, so only an exact,
// non-negative integer may become a DOM path index or text offset.
static bool source_position_index_from_item(ItemReader value,
                                            uint32_t max_value,
                                            uint32_t* out) {
    if (!out) return false;
    TypeId type = value.getType();
    if (type == LMD_TYPE_DECIMAL) {
        int64_t signed_value = 0;
        if (!decimal_to_int64_exact(value.item(), &signed_value) ||
            signed_value < 0 || (uint64_t)signed_value > max_value) {
            return false;
        }
        *out = (uint32_t)signed_value; // INT_CAST_OK: exact source coordinate is uint32_t-bounded.
        return true;
    }
    if (type == LMD_TYPE_INT || type == LMD_TYPE_INT64) {
        int64_t signed_value = value.asInt();
        if (signed_value < 0 || (uint64_t)signed_value > max_value) return false;
        *out = (uint32_t)signed_value; // INT_CAST_OK: validated source coordinate is uint32_t-bounded.
        return true;
    }
    if (type == LMD_TYPE_UINT64) {
        uint64_t unsigned_value = value.asUInt64();
        if (unsigned_value > max_value) return false;
        *out = (uint32_t)unsigned_value; // INT_CAST_OK: validated source coordinate is uint32_t-bounded.
        return true;
    }
    if (type != LMD_TYPE_FLOAT &&
        type != LMD_TYPE_NUM_SIZED) {
        return false;
    }
    double numeric_value = it2d(value.item());
    if (!isfinite(numeric_value) || numeric_value < 0.0 ||
        numeric_value > (double)max_value || floor(numeric_value) != numeric_value) {
        return false;
    }
    *out = (uint32_t)numeric_value; // INT_CAST_OK: finite integral source coordinate is uint32_t-bounded.
    return true;
}

} // namespace

// Pull `{ path: [int...], offset: int }` out of an Item; treat it as a
// SOURCE_POS_TEXT position (the only kind editor selections produce).
// Caller must source_pos_free() the result.
bool source_pos_from_item(Item pos_item, SourcePosC* out) {
    if (!out) return false;
    out->kind = SOURCE_POS_TEXT;
    out->offset = 0;
    source_path_init(&out->path);

    MapReader m = MapReader::fromItem(pos_item);
    if (!m.isValid()) return false;

    ItemReader off = m.get("offset");
    if (!source_position_index_from_item(off, UINT32_MAX, &out->offset)) return false;

    ItemReader path = m.get("path");
    if (!path.isArray()) return true;  // empty path — root
    ArrayReader arr = path.asArray();
    int depth = (int)arr.length();
    if (depth <= 0) return true;
    out->path.indices = (int*)mem_alloc(sizeof(int) * (size_t)depth, MEM_CAT_RENDER);
    if (!out->path.indices) return false;
    out->path.depth = depth;
    for (int i = 0; i < depth; i++) {
        ItemReader idx = arr.get(i);
        uint32_t path_index = 0;
        if (!source_position_index_from_item(idx, INT_MAX, &path_index)) {
            source_path_free(&out->path);
            return false;
        }
        out->path.indices[i] = (int)path_index; // INT_CAST_OK: validated source path index is INT_MAX-bounded.
    }
    return true;
}

bool source_path_from_item(Item path_item, SourcePathC* out) {
    if (!out) return false;
    source_path_init(out);
    ArrayReader arr = ArrayReader::fromItem(path_item);
    if (!arr.isValid()) return false;
    int depth = (int)arr.length();
    if (depth <= 0) return true;
    out->indices = (int*)mem_alloc(sizeof(int) * (size_t)depth, MEM_CAT_RENDER);
    if (!out->indices) return false;
    out->depth = depth;
    for (int i = 0; i < depth; i++) {
        ItemReader idx = arr.get(i);
        uint32_t path_index = 0;
        if (!source_position_index_from_item(idx, INT_MAX, &path_index)) {
            source_path_free(out);
            return false;
        }
        out->indices[i] = (int)path_index; // INT_CAST_OK: validated source path index is INT_MAX-bounded.
    }
    return true;
}

