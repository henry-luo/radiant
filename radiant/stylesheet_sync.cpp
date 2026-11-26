#include "stylesheet_sync.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/css_engine.hpp"
#include "lambda/input/css/css_parser.hpp"
#include "lib/mem_grow.hpp"
#include "lib/strbuf.h"
#include "lib/string.h"
#include <string.h>

static CssStylesheet* stylesheet_parse_inline(DomElement* elem, CssStylesheet* previous) {
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
    const char* source = css_text->str ? css_text->str : "";
    const size_t source_length = strlen(source);
    const char* media = elem->get_attribute("media");
    // an unchanged style can return with its retained DOM fragment. CSSOM
    // handles to a detached sheet must keep their original identity and rules.
    if (previous && previous->owner_document == elem->doc && !previous->cssom_exposed &&
        previous->source_text && previous->source_length == source_length &&
        memcmp(previous->source_text, source, source_length) == 0 &&
        ((!previous->media && !media) || (previous->media && media &&
                                        strcmp(previous->media, media) == 0))) {
        strbuf_free(css_text);
        return previous;
    }

    // CSSOM exposes the sheet immediately after a style node is connected;
    // the post-script stylesheet rescan happens too late for libraries that
    // insert rules during the same script turn.
    CssEngine* engine = css_engine_create(pool);
    CssStylesheet* sheet = engine
        ? css_parse_stylesheet(engine, source, "<inline-style>")
        : nullptr;
    if (engine) css_engine_destroy(engine);
    if (sheet) {
        sheet->source_text = pool_strdup(pool, source);
        sheet->source_length = source_length;
    }
    strbuf_free(css_text);
    if (!sheet) return nullptr;

    sheet->owner_element = elem;
    sheet->owner_document = elem->doc;
    sheet->media = media ? pool_strdup(pool, media) : nullptr;
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

bool radiant_append_document_stylesheet(DomDocument* doc,
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

static void stylesheet_invalidate_dependents(DomDocument* doc, CssStylesheet* sheet) {
    // both registries consume top-level at-rules. Ordinary style changes only
    // invalidate the cascade, which otherwise reparses every unrelated keyframe.
    for (size_t i = 0; i < sheet->rule_count; i++) {
        const CssRule* rule = sheet->rules[i];
        if (!rule) continue;
        if (rule->type == CSS_RULE_KEYFRAMES) doc->services.keyframe_registry = nullptr;
        if (rule->type == CSS_RULE_FONT_FACE) doc->font_faces_processed = false;
    }
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
        stylesheet_invalidate_dependents(doc, sheet);
        return true;
    }
    return false;
}

bool radiant_sync_inline_stylesheet(DomElement* elem) {
    if (!elem || !elem->doc || !elem->tag_name() ||
        str_icmp_cstr(elem->tag_name(), "style") != 0) {
        return false;
    }

    DomDocument* doc = elem->doc;
    ShadowStyle* state = elem->shadow_style();
    CssStylesheet* previous = state ? state->inline_stylesheet.get() : nullptr;
    bool connected = dom_element_is_connected(elem);
    if (!connected) {
        if (previous && previous->cssom_exposed) state->inline_stylesheet = nullptr;
        return stylesheet_remove_inline(doc, elem) || previous;
    }

    CssStylesheet* sheet = stylesheet_parse_inline(elem, previous);
    if (!sheet) return false;
    if (!state) state = elem->ensure_shadow_style();
    if (state) state->inline_stylesheet = lam::up(sheet);
    if (dom_shadow_root(elem)) {
        // A sheet inside a shadow root is associated with its owner, outside
        // the Document's ordered sheet list. Moving the owner changes its scope.
        if (!state) return false;
        stylesheet_remove_inline(doc, elem);
        return true;
    }
    for (int i = 0; i < doc->stylesheet_count; i++) {
        CssStylesheet* current = doc->stylesheets[i];
        if (current && current->owner_element == elem) {
            if (current != sheet) stylesheet_invalidate_dependents(doc, current);
            // a retained sheet can change precedence when its owner moves.
            stylesheet_invalidate_dependents(doc, sheet);
            doc->stylesheets[i] = sheet;
            stylesheet_reorder_document(doc);
            return true;
        }
    }

    if (!radiant_append_document_stylesheet(doc, sheet)) {
        return false;
    }
    stylesheet_invalidate_dependents(doc, sheet);
    return true;
}

CssStylesheet* radiant_shadow_inline_stylesheet(DomElement* owner) {
    const ShadowStyle* state = owner ? owner->shadow_style() : nullptr;
    CssStylesheet* sheet = state ? state->inline_stylesheet.get() : nullptr;
    return sheet && sheet->owner_document == owner->doc && dom_element_is_connected(owner) &&
        dom_shadow_root(owner) ? sheet : nullptr;
}

CssStylesheet* radiant_next_shadow_stylesheet(DomElement* root, DomElement* after) {
    if (!root) return nullptr;
    // CSSOM and the cascade see the same source order and tree boundary.
    for (DomNode* node = dom_source_next_in_tree(root, after ? after : root); node;
         node = dom_source_next_in_tree(root, node)) {
        if (CssStylesheet* sheet = radiant_shadow_inline_stylesheet(node->as_element())) return sheet;
    }
    return nullptr;
}

void radiant_sync_mutated_inline_stylesheets(DomDocument* doc) {
    if (!doc) return;
    for (int i = 0; i < doc->js.inline_stylesheet_mutation_count; i++) {
        DomElement* owner = doc->js.inline_stylesheet_mutations[i];
        if (owner && radiant_sync_inline_stylesheet(owner)) {
            // a later CSSOM edit must not be overwritten by the same text-tree update.
            // retain the batch count so reconciliation still sees a stylesheet mutation.
            doc->js.inline_stylesheet_mutations[i] = nullptr;
        }
    }
    // CSS Cascade source order follows the current document tree, including
    // an inserted style that precedes a previously parsed link or style sheet.
    stylesheet_reorder_document(doc);
}

