// source_pos_bridge.cpp — C-side scaffolding for the editor source-position
// bridge (Phase R7, Radiant integration step 1). The pure-Lambda half lives
// in lambda/editor/mod_dom_bridge.ls.
//
// The native SourcePathC / SourcePosC lifecycle lives in source_path.cpp.
// The optional host owns the render-map path table and returns owned path copies.
// This bridge provides:
//   * `source_pos_from_dom_boundary`: walks DOM ancestors from a boundary
//     point, finds the first DomElement whose Lambda backing is registered
//     in render_map, retrieves its recorded source path, and (for text
//     hits) converts the boundary's UTF-16 code-unit offset to a code-point
//     offset, the unit Lambda strings and the editor model index by (S2.5.8).
//   * `dom_boundary_from_source_pos` resolves a source path back to a DOM
//     boundary using the recorded render-map paths.

// Bring in the full Item definition before including the bridge header,
// since Item is passed by value across the C ABI.
#include "../lambda/core/data.hpp"

#include "event.hpp"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "../lib/memtrack.h"
#include "tagged.hpp"
#include "../lambda/core/mark_reader.hpp"
#include "source_position_host.hpp"
#include "../lambda/input/css/dom_node.hpp"
#include "../lambda/input/css/dom_element.hpp"

// The full Radiant binary links state_store.cpp and uses the canonical writer.
// Source-position bridge unit tests link this file without the full StateStore
// stack, so provide a weak compatibility fallback for that narrower target.
extern "C" bool state_store_set_selection(DocState* state,
                                           const DomBoundary* anchor,
                                           const DomBoundary* focus,
                                           const char** out_exception);
extern "C" __attribute__((weak)) bool state_store_set_selection(
        DocState* state,
        const DomBoundary* anchor,
        const DomBoundary* focus,
        const char** out_exception) {
    if (!state || !state->dom_selection) {
        if (out_exception) *out_exception = "InvalidStateError";
        return false;
    }
    DomSelection* selection = state->dom_selection;
    if (!anchor || !focus || !anchor->node || !focus->node) {
        dom_selection_remove_all_ranges(selection);
        return true;
    }
    if (dom_boundary_compare(anchor, focus) == DOM_BOUNDARY_EQUAL) {
        return dom_selection_collapse(selection, anchor->node, anchor->offset,
                                      out_exception);
    }
    return dom_selection_set_base_and_extent(selection,
        anchor->node, anchor->offset, focus->node, focus->offset,
        out_exception);
}

static bool source_item_is_editor_kind(Item item, const char* expected) {
    uintptr_t raw = (uintptr_t)item.item;
    if (raw == 0 || item._type_id != 0 || (raw & ITEM_DBL_MASK) ||
        raw < 0x10000ULL || (raw & (sizeof(void*) - 1)) != 0) {
        // render-map tests and handler dispatch can use synthetic Item keys;
        // only real direct-pointer map Items are safe to inspect as maps here.
        return false;
    }
    MapReader m = MapReader::fromItem(item);
    if (!m.isValid()) return false;
    ItemReader kind = m.get("kind");
    if (!kind.isSymbol()) return false;
    Symbol* sym = kind.asSymbol();
    return sym && strcmp(sym->chars, expected) == 0;
}

static bool source_item_is_editor_text_leaf(Item item) {
    return source_item_is_editor_kind(item, "text");
}

static bool source_item_is_empty_editor_node(Item item) {
    if (!source_item_is_editor_kind(item, "node")) return false;
    ItemReader content = MapReader::fromItem(item).get("content");
    return content.isArray() && content.asArray().isEmpty();
}

extern "C" {

static bool is_generated_content_node(DomNode* node) {
    // CSS-generated boxes have no child slot or text position in the editor model.
    return dom_is_generated_pseudo_node(node) ||
        (node && node->node_type == DOM_NODE_ELEMENT && node->view_type == RDT_VIEW_MARKER);
}

static uint32_t source_child_offset(DomElement* parent, uint32_t dom_offset) {
    uint32_t index = 0, source_index = 0;
    for (DomNode* child = parent ? parent->first_child : nullptr; child && index < dom_offset;
         child = child->next_sibling, index++) {
        if (!is_generated_content_node(child)) source_index++;
    }
    return source_index;
}

static int source_child_index(DomNode* node) {
    if (!node || !node->parent || !node->parent->is_element()) return -1;
    return (int)source_child_offset(node->parent->as_element(), dom_node_child_index(node)); // INT_CAST_OK: DOM child counts fit source path indices.
}

static DomNode* source_child_at(DomElement* de, int target_idx) {
    if (!de || target_idx < 0) return NULL;
    int idx = 0;
    for (DomNode* c = de->first_child; c; c = c->next_sibling) {
        if (is_generated_content_node(c)) continue;
        if (idx == target_idx) return c;
        idx++;
    }
    return NULL;
}

// ---------------------------------------------------------------------------
// DOM → source position
// ---------------------------------------------------------------------------
// Walk up DOM ancestry from `boundary->node` until a DomElement is found
// whose Lambda backing is registered in render_map. The recorded path
// becomes the SourcePos path; the offset is:
//   * for a text-node boundary: the code-point offset within that text
//     leaf, with the path extended by the text node's child index in its
//     parent;
//   * for an element-node boundary: the boundary's offset (already a
//     child index) extended onto the path.

bool source_pos_from_dom_boundary(const DomBoundary* boundary,
                                  SourcePosC* out) {
    if (!out) return false;
    source_path_init(&out->path);
    out->offset = 0;
    out->kind = SOURCE_POS_TEXT;
    if (!boundary || !boundary->node) return false;

    DomNode* node = boundary->node;
    int leaf_child_index = -1;     // index of `node` within first registered ancestor
    bool is_text_leaf = (node->node_type == DOM_NODE_TEXT);
    uint32_t leaf_cp_offset = 0;

    if (is_text_leaf) {
        DomText* tn = lam::dom_require_text(node);
        leaf_cp_offset = dom_text_utf16_to_codepoint(tn, boundary->offset);
        leaf_child_index = source_child_index(node);
        node = node->parent;
        if (!node) return false;
    }

    // Walk upward until we find a DomElement registered in the render map.
    // Literal wrappers generated inside a template can have Lambda backing
    // without their own render-map entry, so keep climbing through them.
    Item source_item;
    SourcePathC base;
    bool found_base = false;
    while (node) {
        if (node->node_type == DOM_NODE_ELEMENT) {
            DomElement* de = lam::dom_require_element(node);
            if (!de->is_synthetic()) {
                Item result_item;
                result_item.element = dom_element_render_source(de);
                if (radiant_source_position_lookup(result_item, &source_item, &base)) {
                    found_base = true;
                    break;
                }
            }
        }
        if (is_text_leaf) {
            leaf_child_index = source_child_index(node);
        }
        node = node->parent;
    }
    if (!found_base) return false;
    if (source_item_is_empty_editor_node(source_item)) {
        // a rendered placeholder has no source child or text offset; reentry names its empty container.
        out->path = base;
        out->offset = 0;
        out->kind = SOURCE_POS_ELEMENT;
        return true;
    }
    // base is a clone of the recorded path. Text leaves rendered through
    // mark wrappers (<strong>/<em>/<code>/...) are recorded at the source
    // text leaf itself, so keep that path instead of appending the wrapper's
    // DOM child index. Container nodes still need the child-index extension.
    int extra = 0;
    int extra_indices[2];
    bool recorded_text_leaf = is_text_leaf &&
        source_item_is_editor_text_leaf(source_item);
    if (is_text_leaf && !recorded_text_leaf) {
        if (leaf_child_index < 0) {
            source_path_free(&base);
            return false;
        }
        extra_indices[extra++] = leaf_child_index;
    }

    int total = base.depth + extra;
    int* combined = (int*)mem_alloc(sizeof(int) * (size_t)(total > 0 ? total : 1), MEM_CAT_RENDER);
    if (!combined) { source_path_free(&base); return false; }
    if (base.depth) memcpy(combined, base.indices, sizeof(int) * (size_t)base.depth);
    for (int i = 0; i < extra; i++) combined[base.depth + i] = extra_indices[i];
    source_path_free(&base);

    out->path.indices = combined;
    out->path.depth = total;
    if (is_text_leaf) {
        out->offset = leaf_cp_offset;
        out->kind = SOURCE_POS_TEXT;
    } else {
        // generated markers consume DOM child slots, but never editor child slots.
        out->offset = source_child_offset(boundary->node->as_element(), boundary->offset);
        out->kind = SOURCE_POS_ELEMENT;
    }
    return true;
}

bool source_pos_from_dom_range(const DomRange* range,
                               SourcePosC* out_start,
                               SourcePosC* out_end) {
    if (!range || !out_start || !out_end) return false;
    DomBoundary s = range->start;
    DomBoundary e = range->end;
    bool ok1 = source_pos_from_dom_boundary(&s, out_start);
    bool ok2 = source_pos_from_dom_boundary(&e, out_end);
    if (!ok1 || !ok2) {
        source_pos_free(out_start);
        source_pos_free(out_end);
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Source position → DOM
// ---------------------------------------------------------------------------
// Walk the DOM subtree rooted at `dom_root`, looking for the DomElement
// whose recorded source path (stored at apply() time via
// render_map_record_path) matches the desired source path:
//   * SOURCE_POS_ELEMENT: recorded path == pos->path; boundary points at
//     that element with offset = pos->offset (a child index, identical
//     between source and DOM child lists by editor convention).
//   * SOURCE_POS_TEXT: recorded path == pos->path with the last index
//     dropped; the dropped index is the text node's position among that
//     element's children; offset is converted code points → UTF-16.

static bool path_equals_prefix(const SourcePathC* path, int prefix_len,
                               const SourcePathC* other) {
    if (!path || !other) return false;
    if (prefix_len < 0 || prefix_len > path->depth) return false;
    if (other->depth != prefix_len) return false;
    if (prefix_len == 0) return true;
    return memcmp(path->indices, other->indices,
                  sizeof(int) * (size_t)prefix_len) == 0;
}

static DomText* first_text_descendant(DomNode* node) {
    if (!node || is_generated_content_node(node)) return NULL;
    if (node->node_type == DOM_NODE_TEXT) return lam::dom_require_text(node);
    if (node->node_type != DOM_NODE_ELEMENT) return NULL;
    DomElement* el = lam::dom_require_element(node);
    for (DomNode* c = el->first_child; c; c = c->next_sibling) {
        DomText* hit = first_text_descendant(c);
        if (hit) return hit;
    }
    return NULL;
}

static DomNode* source_descendant_at(DomElement* root,
                                     const SourcePathC* path,
                                     int start_depth) {
    if (!root || !path || start_depth < 0 || start_depth > path->depth) {
        return NULL;
    }
    DomNode* node = static_cast<DomNode*>(root);
    for (int i = start_depth; i < path->depth; i++) {
        if (node->node_type != DOM_NODE_ELEMENT) return NULL;
        node = source_child_at(lam::dom_require_element(node), path->indices[i]);
        if (!node) return NULL;
    }
    return node;
}

static uint32_t source_selection_child_count(DomNode* node) {
    uint32_t child_count = 0;
    if (node && node->is_element()) {
        for (DomNode* child = node->as_element()->first_child; child;
             child = child->next_sibling) {
            child_count++;
        }
    }
    return child_count;
}

// Resolve a SourcePos against a single DomElement candidate. Reads its
// recorded source path; on a match, fills the boundary and source-path depth.
static bool try_resolve_at_element(DomElement* de, const SourcePosC* pos,
                                   DomBoundary* out, int* out_depth) {
    if (!de || de->is_synthetic()) return false;
    Item result_item;
    result_item.element = dom_element_render_source(de);
    Item source_item;
    SourcePathC recorded;
    if (!radiant_source_position_lookup(result_item, &source_item, &recorded)) {
        return false;
    }
    bool matched = false;
    if (pos->kind == SOURCE_POS_ELEMENT) {
        if (path_equals_prefix(&pos->path, pos->path.depth, &recorded)) {
            out->node = static_cast<DomNode*>(de);
            out->offset = pos->offset;  // child index in source == DOM child index
            matched = true;
        }
    } else { // SOURCE_POS_TEXT
        if (source_path_equal(&pos->path, &recorded)) {
            if (source_item_is_editor_kind(source_item, "node")) {
                DomText* placeholder = pos->offset == 0 && source_item_is_empty_editor_node(source_item)
                    ? first_text_descendant(de) : nullptr;
                if (placeholder) {
                    // an empty model block projects a presentation-only leaf that owns its visible caret.
                    out->node = placeholder;
                    out->offset = 0;
                } else {
                    // SourcePos has no kind field: a model container's offset counts children, not code points.
                    DomNode* child = pos->offset <= INT_MAX ? source_child_at(de, pos->offset) : nullptr;
                    out->node = de;
                    out->offset = child ? dom_node_child_index(child) : source_selection_child_count(de);
                }
                matched = true;
            } else {
            DomText* tn = first_text_descendant(static_cast<DomNode*>(de));
            if (tn) {
                out->node = static_cast<DomNode*>(tn);
                out->offset = dom_text_codepoint_to_utf16(tn, pos->offset);
                matched = true;
            }
            }
        } else if (recorded.depth < pos->path.depth &&
                   path_equals_prefix(&pos->path, recorded.depth, &recorded)) {
            // Nested apply() output can omit render-map entries for an
            // intermediate source node, so resolve the remaining path below
            // its recorded ancestor instead of abandoning the caret update.
            DomNode* child = source_descendant_at(de, &pos->path,
                                                   recorded.depth);
            DomText* tn = first_text_descendant(child);
            if (tn) {
                out->node = static_cast<DomNode*>(tn);
                out->offset = dom_text_codepoint_to_utf16(tn, pos->offset);
                matched = true;
            }
        }
    }
    if (matched) *out_depth = recorded.depth;
    source_path_free(&recorded);
    return matched;
}

// Generated wrappers can change DOM child indices. Prefer the deepest mapped
// source owner over an ancestor's structural fallback before restoring a caret.
static void resolve_walk(DomNode* node, const SourcePosC* pos,
                         DomBoundary* out, int* best_depth) {
    if (!node) return;
    if (node->node_type == DOM_NODE_ELEMENT) {
        DomElement* de = lam::dom_require_element(node);
        DomBoundary candidate{nullptr, 0};
        int depth = -1;
        if (try_resolve_at_element(de, pos, &candidate, &depth) && depth > *best_depth) {
            *out = candidate;
            *best_depth = depth;
        }
        if (*best_depth == pos->path.depth) return;
        for (DomNode* c = de->first_child; c; c = c->next_sibling) {
            resolve_walk(c, pos, out, best_depth);
            if (*best_depth == pos->path.depth) return;
        }
    }
}

bool dom_boundary_from_source_pos(DomNode* dom_root,
                                  const SourcePosC* pos,
                                  DomBoundary* out) {
    if (!dom_root || !pos || !out) return false;
    out->node = NULL;
    out->offset = 0;
    int best_depth = -1;
    resolve_walk(dom_root, pos, out, &best_depth);
    return best_depth >= 0;
}

} // extern "C"

// Resolve a SOURCE_POS_ELEMENT path to the DomElement it refers to, by
// walking the DOM (via dom_boundary_from_source_pos with offset 0 and
// kind=ELEMENT). Returns NULL on failure.
static DomNode* dom_node_from_source_path(DomNode* dom_root, const SourcePathC* path) {
    if (!dom_root || !path) return NULL;
    SourcePosC p;
    p.path = *path;          // borrow indices — caller still owns
    p.offset = 0;
    p.kind = SOURCE_POS_ELEMENT;
    DomBoundary b = {0};
    if (!dom_boundary_from_source_pos(dom_root, &p, &b)) return NULL;
    return b.node;
}


extern "C" bool dom_selection_apply_source_selection(DomSelection* ds,
                                                     DomNode* dom_root,
                                                     Item selection) {
    if (!ds || !dom_root) return false;

    MapReader m = MapReader::fromItem(selection);
    if (!m.isValid()) return false;

    ItemReader kind = m.get("kind");
    if (!kind.isSymbol()) return false;
    const char* kind_str = kind.asSymbol() ? kind.asSymbol()->chars : NULL;
    if (!kind_str) return false;

    const char* exc = NULL;

    if (strcmp(kind_str, "text") == 0) {
        SourcePosC anchor_pos;
        SourcePosC head_pos;
        if (!source_pos_from_item(m.get("anchor").item(), &anchor_pos)) return false;
        if (!source_pos_from_item(m.get("head").item(),   &head_pos)) {
            source_pos_free(&anchor_pos);
            return false;
        }
        DomBoundary anchor_b = {0};
        DomBoundary head_b = {0};
        bool ok = dom_boundary_from_source_pos(dom_root, &anchor_pos, &anchor_b)
               && dom_boundary_from_source_pos(dom_root, &head_pos,   &head_b);
        source_pos_free(&anchor_pos);
        source_pos_free(&head_pos);
        if (!ok || !anchor_b.node || !head_b.node) return false;
        if (!ds->state) return false;
        return state_store_set_selection(ds->state, &anchor_b, &head_b, &exc);
    }

    if (strcmp(kind_str, "node") == 0) {
        SourcePathC path;
        if (!source_path_from_item(m.get("path").item(), &path)) return false;
        DomNode* node = dom_node_from_source_path(dom_root, &path);
        source_path_free(&path);
        if (!node) return false;
        if (!ds->state) return false;
        // selecting an atomic image's empty contents collapses the range; select the node itself.
        if (!node->parent) return false;
        uint32_t index = dom_node_child_index(node);
        DomBoundary start = { node->parent, index };
        DomBoundary end = { node->parent, index + 1 };
        return state_store_set_selection(ds->state, &start, &end, &exc);
    }

    if (strcmp(kind_str, "all") == 0) {
        if (!ds->state) return false;
        SourcePathC root_path;
        source_path_init(&root_path);
        DomNode* source_root = dom_node_from_source_path(dom_root, &root_path);
        DomNode* selection_root = source_root ? source_root : dom_root;
        DomBoundary start = { selection_root, 0 };
        DomBoundary end = { selection_root, source_selection_child_count(selection_root) };
        return state_store_set_selection(ds->state, &start, &end, &exc);
    }

    return false;
}
