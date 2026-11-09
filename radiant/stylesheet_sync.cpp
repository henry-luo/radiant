#include "stylesheet_sync.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/css_engine.hpp"
#include "lambda/input/css/css_parser.hpp"
#include "lib/mem_grow.hpp"
#include "lib/strbuf.h"
#include "lib/string.h"
#include <string.h>

static CssStylesheet* stylesheet_parse_inline(DomElement* elem) {
    if (!elem || !elem->doc || !elem->doc->document_pool) return nullptr;

    Pool* pool = elem->doc->document_pool;
    StrBuf* css_text = strbuf_new_cap(32);
    if (!css_text) return nullptr;
    for (DomNode* child = elem->first_child; child; child = child->next_sibling) {
        if (!child->is_text()) continue;
        DomText* text = child->as_text();
        if (text && text->text && text->length > 0) {
            strbuf_append_str_n(css_text, text->text, text->length);
        }
    }

    // CSSOM exposes the sheet immediately after a style node is connected;
    // the post-script stylesheet rescan happens too late for libraries that
    // insert rules during the same script turn.
    CssEngine* engine = css_engine_create(pool);
    CssStylesheet* sheet = engine
        ? css_parse_stylesheet(engine, css_text->str ? css_text->str : "", "<inline-style>")
        : nullptr;
    if (engine) css_engine_destroy(engine);
    strbuf_free(css_text);
    if (!sheet) return nullptr;

    sheet->owner_element = elem;
    return sheet;
}

static bool stylesheet_node_precedes(DomNode* first, DomNode* second) {
    if (!first || !second || first == second) return false;
    int first_depth = 0;
    int second_depth = 0;
    for (DomNode* node = first; node; node = node->parent) first_depth++;
    for (DomNode* node = second; node; node = node->parent) second_depth++;
    while (first_depth > second_depth) {
        first = first->parent;
        first_depth--;
    }
    if (first == second) return false;
    while (second_depth > first_depth) {
        second = second->parent;
        second_depth--;
    }
    if (first == second) return true;
    while (first && second && first->parent != second->parent) {
        first = first->parent;
        second = second->parent;
    }
    if (!first || !second || first->parent != second->parent) return false;
    for (DomNode* node = first; node; node = node->next_sibling) {
        if (node == second) return true;
    }
    return false;
}

static void stylesheet_reorder_document(DomDocument* doc) {
    if (!doc || !doc->stylesheets) return;
    for (int i = 1; i < doc->stylesheet_count; i++) {
        for (int j = i; j > 0; j--) {
            CssStylesheet* previous = doc->stylesheets[j - 1];
            CssStylesheet* current = doc->stylesheets[j];
            if (!previous || !current || !previous->owner_element ||
                !current->owner_element ||
                !stylesheet_node_precedes(current->owner_element,
                                         previous->owner_element)) {
                break;
            }
            doc->stylesheets[j - 1] = current;
            doc->stylesheets[j] = previous;
        }
    }
}

static bool stylesheet_append_document(DomDocument* doc,
                                                  CssStylesheet* sheet) {
    if (!doc || !doc->document_pool || !sheet) return false;
    if (doc->stylesheet_count >= doc->stylesheet_capacity) {
        // Loader-owned sheet arrays can outlive a document but are not owned by
        // its pool, so append by copying instead of reallocating foreign storage.
        if (!lam::pool_copy_grow_array(doc->document_pool, &doc->stylesheets,
                                       &doc->stylesheet_capacity, doc->stylesheet_count,
                                       doc->stylesheet_count + 1, 4, false)) return false;
    }
    doc->stylesheets[doc->stylesheet_count++] = sheet;
    stylesheet_reorder_document(doc);
    return true;
}

static bool stylesheet_remove_inline(DomDocument* doc,
                                                DomElement* elem) {
    if (!doc || !elem) return false;
    for (int i = 0; i < doc->stylesheet_count; i++) {
        CssStylesheet* sheet = doc->stylesheets[i];
        if (!sheet || sheet->owner_element != elem) continue;
        memmove(&doc->stylesheets[i], &doc->stylesheets[i + 1],
                (size_t)(doc->stylesheet_count - i - 1) * sizeof(CssStylesheet*));
        doc->stylesheet_count--;
        // A detached style must stop contributing rules and keyframes immediately.
        doc->services.keyframe_registry = nullptr;
        doc->font_faces_processed = false;
        return true;
    }
    return false;
}

bool radiant_sync_inline_stylesheet(DomElement* elem) {
    if (!elem || !elem->doc || !elem->tag_name ||
        str_icmp_cstr(elem->tag_name, "style") != 0) {
        return false;
    }

    DomDocument* doc = elem->doc;
    bool connected = false;
    for (DomNode* node = static_cast<DomNode*>(elem); node; node = node->parent) {
        if (node == static_cast<DomNode*>(doc->root)) {
            connected = true;
            break;
        }
    }
    if (!connected) return stylesheet_remove_inline(doc, elem);

    CssStylesheet* sheet = stylesheet_parse_inline(elem);
    if (!sheet) return false;
    for (int i = 0; i < doc->stylesheet_count; i++) {
        if (doc->stylesheets[i] && doc->stylesheets[i]->owner_element == elem) {
            doc->stylesheets[i] = sheet;
            stylesheet_reorder_document(doc);
            // Keyframes are parsed from stylesheet text, so their cache tracks
            // the same text-tree update as the style sheet.
            doc->services.keyframe_registry = nullptr;
            doc->font_faces_processed = false;
            return true;
        }
    }

    if (!stylesheet_append_document(doc, sheet)) {
        return false;
    }
    doc->services.keyframe_registry = nullptr;
    doc->font_faces_processed = false;
    return true;
}

void radiant_sync_mutated_inline_stylesheets(DomDocument* doc) {
    if (!doc) return;
    for (int i = 0; i < doc->js.inline_stylesheet_mutation_count; i++) {
        radiant_sync_inline_stylesheet(doc->js.inline_stylesheet_mutations[i]);
    }
    // CSS Cascade source order follows the current document tree, including
    // an inserted style that precedes a previously parsed link or style sheet.
    stylesheet_reorder_document(doc);
}

