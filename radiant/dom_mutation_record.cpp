#include "dom_mutation_record.hpp"
#include "css_mutation_queries.hpp"
#include "event.hpp"
#include "view.hpp"
#include "../lambda/input/css/dom_lifecycle.hpp"
#include "../lib/str.h"
#include "../lib/mem_grow.hpp"
#include "../lib/log.h"
#include <string.h>

static inline uint32_t dom_mutation_bit(DomJsMutationKind kind) {
    uint32_t slot = (uint32_t)kind;
    if (slot >= 31) slot = 0;
    return 1u << slot;
}


static bool dom_mutation_node_was_connected(DomDocument* doc, DomNode* node) {
    if (!doc || !doc->root || !node) return false;
    for (DomNode* current = node; current; current = current->parent) {
        if (current == static_cast<DomNode*>(doc->root)) return true;
    }
    return false;
}

static DomJsMutationAttribute dom_mutation_attribute_from_name(DomDocument* doc, const char* name) {
    if (name && str_icmp_cstr(name, "class") == 0) {
        return DOM_JS_MUTATION_ATTRIBUTE_CLASS;
    }
    // data/ARIA writes cannot change native pseudo states; only an attribute
    // reference inside :has() can make their cascade escape the local subtree.
    if (name && (strncmp(name, "data-", 5) == 0 || strncmp(name, "aria-", 5) == 0) &&
        !dom_engine_attribute_has_relational_css_dependency(doc, name))
        return DOM_JS_MUTATION_ATTRIBUTE_LOCAL;
    return DOM_JS_MUTATION_ATTRIBUTE_UNKNOWN;
}

static bool dom_mutation_attribute_affects_direction(const char* name) {
    // unknown names stay conservative; CSS visibility does not filter first-strong text.
    return !name || str_ieq_cstr(name, "dir") || str_ieq_cstr(name, "value") ||
        str_ieq_cstr(name, "type") || str_ieq_cstr(name, "slot") ||
        str_ieq_cstr(name, "name") || str_istarts_with_cstr(name, "xmlns");
}

static bool dom_mutation_child_list_affects_direction(DomNode* parent) {
    // slot distribution can affect a different tree; retain its conservative closure.
    if (dom_shadow_root(parent)) return true;
    for (DomElement* ancestor = parent ? parent->as_element() : nullptr;
         ancestor; ancestor = dom_parent_element(ancestor)) {
        if (ancestor->shadow_root_element()) return true;
        CssEnum direction = dom_html_direction_state(ancestor);
        if (direction == CSS_VALUE_LTR || direction == CSS_VALUE_RTL) return false;
        if (direction == CSS_VALUE_AUTO || dom_element_is_html_tag(ancestor, MARKUP_NAME_BDI))
            return true;
    }
    return false;
}

void radiant_dom_record_mutation_detail(DomDocument* doc, DomJsMutationKind kind,
                                        DomNode* target, DomNode* parent, uint32_t sequence,
                                        const char* attribute_name, CssPropertyCode presentation_property) {
    parent = dom_source_inclusive_ancestor(parent);
    if (!doc) return;

    if (sequence == 0) {
        sequence = doc->js.mutation_sequence + 1;
    }
    // the reconciler may skip layout only for reflow requests this batch made
    if (doc->js.mutation_record_count == 0 && doc->js.mutation_record_overflow == 0) {
        doc->js.reflow_pending_before_batch = doc->state && doc->state->needs_reflow;
        doc->js.dirty_before_batch = doc->state && doc->state->is_dirty;
        doc->js.full_repaint_requests_before_batch = doc->state ? doc->state->full_repaint_requests : 0;
    }
    doc->js.mutation_kind_mask |= dom_mutation_bit(kind);

    if (dom_js_mutation_records_reserve(doc, doc->js.mutation_record_count + 1)) {
        DomNodeRef target_ref = dom_node_ref(target);
        DomNodeRef parent_ref = dom_node_ref(parent);
        if (target && !dom_node_pin(doc, target_ref, DOM_NODE_PIN_RECONCILE)) return;
        if (parent && !dom_node_pin(doc, parent_ref, DOM_NODE_PIN_RECONCILE)) {
            if (target) dom_node_unpin(doc, target_ref, DOM_NODE_PIN_RECONCILE);
            return;
        }
        DomJsMutationRecord* record =
            &doc->js.mutation_records[doc->js.mutation_record_count++];
        record->sequence = sequence;
        record->kind = kind;
        record->target = target;
        record->parent = parent;
        record->target_id = target_ref.expected_id;
        record->parent_id = parent_ref.expected_id;
        record->attribute = kind == DOM_JS_MUTATION_ATTRIBUTE
            ? dom_mutation_attribute_from_name(doc, attribute_name)
            : DOM_JS_MUTATION_ATTRIBUTE_UNKNOWN;
        record->direction_sensitive = kind == DOM_JS_MUTATION_ATTRIBUTE
            ? dom_mutation_attribute_affects_direction(attribute_name)
            : (kind == DOM_JS_MUTATION_CHILD_INSERT || kind == DOM_JS_MUTATION_CHILD_REMOVE)
                ? dom_mutation_child_list_affects_direction(parent)
                : kind == DOM_JS_MUTATION_TEXT || kind == DOM_JS_MUTATION_CONTROL_VALUE;
        record->presentation_property = presentation_property;
        record->was_connected =
            dom_mutation_node_was_connected(doc, target) ||
            dom_mutation_node_was_connected(doc, parent);
    } else {
        doc->js.mutation_record_overflow++;
    }

    if (target) {
        dom_invalidate_layout_subtree(target);
    }
    if (parent) {
        dom_mark_dirty_ancestors(parent);
    }
}

void radiant_dom_record_inline_stylesheet_owner(DomDocument* doc,
                                               DomElement* style) {
    if (!doc || !style) return;
    DomJsRuntime* js = &doc->js;
    for (int i = 0; i < js->inline_stylesheet_mutation_count; i++) {
        if (js->inline_stylesheet_mutations[i] == style) return;
    }
    if (js->inline_stylesheet_mutation_count >=
        js->inline_stylesheet_mutation_capacity) {
        if (!lam::pool_copy_grow_array(doc->document_pool,
                                       &js->inline_stylesheet_mutations,
                                       &js->inline_stylesheet_mutation_capacity,
                                       js->inline_stylesheet_mutation_count,
                                       js->inline_stylesheet_mutation_count + 1, 4, false)) {
            log_error("dom mutation: failed to retain inline stylesheet owner");
            return;
        }
    }
    js->inline_stylesheet_mutations[js->inline_stylesheet_mutation_count++] = style;
}

static void dom_record_inline_stylesheets_in_subtree(DomDocument* doc,
                                                      DomNode* node) {
    if (!node) return;
    if (node->is_element()) {
        DomElement* element = static_cast<DomElement*>(node);
        if (element->tag_name && str_icmp_cstr(element->tag_name, "style") == 0) {
            radiant_dom_record_inline_stylesheet_owner(doc, element);
        }
        for (DomNode* child = element->first_child; child; child = child->next_sibling) {
            dom_record_inline_stylesheets_in_subtree(doc, child);
        }
    }
}

static DomElement* dom_inline_stylesheet_mutation_owner(DomNode* target,
                                                         DomNode* parent) {
    DomNode* candidates[] = {target, parent};
    for (DomNode* node : candidates) {
        if (!node) continue;
        DomElement* element = node->is_element()
            ? static_cast<DomElement*>(node)
            : (node->parent && node->parent->is_element()
                ? static_cast<DomElement*>(node->parent) : nullptr);
        if (element && element->tag_name && str_icmp_cstr(element->tag_name, "style") == 0) {
            return element;
        }
    }
    return nullptr;
}

void radiant_dom_record_inline_stylesheet_mutation(DomDocument* doc,
                                                   DomJsMutationKind kind,
                                                   DomNode* target,
                                                   DomNode* parent) {
    DomJsRuntime* js = doc ? &doc->js : nullptr;
    if (!js || (kind != DOM_JS_MUTATION_CHILD_INSERT &&
                kind != DOM_JS_MUTATION_CHILD_REMOVE &&
                kind != DOM_JS_MUTATION_TREE_REPLACE &&
                kind != DOM_JS_MUTATION_TEXT)) {
        return;
    }
    if (kind == DOM_JS_MUTATION_CHILD_INSERT ||
        kind == DOM_JS_MUTATION_CHILD_REMOVE ||
        kind == DOM_JS_MUTATION_TREE_REPLACE) {
        // A subtree replacement can introduce several <style> descendants.
        dom_record_inline_stylesheets_in_subtree(doc, target);
    }
    DomElement* style = dom_inline_stylesheet_mutation_owner(target, parent);
    if (!style) return;
    radiant_dom_record_inline_stylesheet_owner(doc, style);
}

void radiant_dom_mutation_begin(DomDocument* doc, DomJsMutationKind kind,
                                DomNode* target, DomNode* parent, const char* attribute_name,
                                CssPropertyCode presentation_property) {
    if (!doc) return;
    parent = dom_source_inclusive_ancestor(parent);
    doc->js.mutation_count++;
    doc->js.mutation_sequence++;
    doc->mutation_epoch++;
    bool inherited_change = presentation_property == CSS_PROPERTY_UNKNOWN ||
        css_property_runtime_inherited(presentation_property);
    if (inherited_change) doc->style_content_epoch++;
    dom_layer_note_mutation(target, !inherited_change);
    if (parent && (!target || parent != target->parent)) dom_layer_note_mutation(parent);
    radiant_dom_record_inline_stylesheet_mutation(doc, kind, target, parent);

    bool has_pending_structural_record = false;
    if (doc->js.mutation_record_count > 0) {
        DomJsMutationRecord* last = &doc->js.mutation_records[doc->js.mutation_record_count - 1];
        bool legacy_flush = kind == DOM_JS_MUTATION_UNKNOWN && !target && !parent;
        bool same_detail = last->kind == kind && last->target == target && last->parent == parent;
        // Pre-remove and post-insert may have published this structural record already.
        has_pending_structural_record = last->sequence == doc->js.mutation_sequence &&
            (legacy_flush || same_detail);
    }
    if (!has_pending_structural_record) {
        radiant_dom_record_mutation_detail(doc, kind, target, parent, doc->js.mutation_sequence,
                                           attribute_name, presentation_property);
    }
}

void radiant_dom_mutation_finish(DomDocument* doc, DomJsMutationKind kind) {
    DocState* state = doc ? doc->state : nullptr;
    if (!state) return;
    view_state_prune_orphans(state);
    // Paint-only mutations still need a style turn in a transient CLI document.
    if (kind != DOM_JS_MUTATION_STYLE_REPAINT ||
        (doc->js.host_ui_context && !doc->js.host_driven_loop)) {
        doc_state_request_reflow(state);
    }
}

void radiant_dom_notify_native(DomDocument* doc, DomJsMutationKind kind,
                               DomNode* target, DomNode* parent,
                               const char* attribute_name, const char* old_value) {
    if (!doc) doc = dom_node_ancestor_document(target);
    if (!doc) doc = dom_node_ancestor_document(parent);
    if (!doc || dom_edit_transaction_defer_mutation(doc, kind, target, parent,
                                                     attribute_name, old_value)) return;
    radiant_dom_mutation_begin(doc, kind, target, parent, attribute_name, CSS_PROPERTY_UNKNOWN);
    radiant_dom_mutation_finish(doc, kind);
}

