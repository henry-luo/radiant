#include "dom_mutation_record.hpp"
#include "css_mutation_queries.hpp"
#include "event.hpp"
#include "event_host.hpp"
#include "view.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"
#include "lib/str.h"
#include "lib/mem_grow.hpp"
#include "lib/log.h"
#include <string.h>

static inline uint32_t dom_mutation_bit(DomJsMutationKind kind) {
    uint32_t slot = (uint32_t)kind;
    if (slot >= 31) slot = 0;
    return 1u << slot;
}


static DomJsMutationAttribute dom_mutation_attribute_from_name(DomDocument* doc,
    DomNode* target, const char* name, StrView old_value) {
    if (name && str_icmp_cstr(name, "class") == 0) {
        DomElement* element = target ? target->as_element() : nullptr;
        // unchanged relational memberships cannot restyle an ancestor; shadow
        // stylesheet graphs and unknown old values retain the conservative path.
        if (!doc->js.has_shadow_roots && old_value.str && element &&
            !radiant_css_document_relational_attribute_match_changes(doc, name, old_value,
                element->get_attribute_view(name))) return DOM_JS_MUTATION_ATTRIBUTE_LOCAL;
        return DOM_JS_MUTATION_ATTRIBUTE_CLASS;
    }
    if (doc->js.has_shadow_roots && name &&
        (str_ieq_cstr(name, "slot") || (str_ieq_cstr(name, "name") && target &&
            dom_element_is_html_tag(target->as_element(), MARKUP_NAME_SLOT)))) {
        return DOM_JS_MUTATION_ATTRIBUTE_SLOT_DISTRIBUTION;
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

static void dom_record_mutation_detail(DomDocument* doc, DomJsMutationKind kind,
                                        DomNode* target, DomNode* parent, uint32_t sequence,
                                        const char* attribute_name, CssPropertyCode presentation_property,
                                        StrView old_value) {
    parent = dom_source_inclusive_ancestor(parent);
    if (!doc) return;
    // Atomic replacements may publish only their child records before the
    // final notification. The sheets belong to each recorded child mutation.
    radiant_dom_record_inline_stylesheet_mutation(doc, kind, target, parent);

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
            ? dom_mutation_attribute_from_name(doc, target, attribute_name, old_value)
            : DOM_JS_MUTATION_ATTRIBUTE_UNKNOWN;
        record->direction_sensitive = kind == DOM_JS_MUTATION_ATTRIBUTE
            ? dom_mutation_attribute_affects_direction(attribute_name)
            : (kind == DOM_JS_MUTATION_CHILD_INSERT || kind == DOM_JS_MUTATION_CHILD_REMOVE)
                ? dom_mutation_child_list_affects_direction(parent)
                : kind == DOM_JS_MUTATION_TEXT || kind == DOM_JS_MUTATION_CONTROL_VALUE;
        record->presentation_property = presentation_property;
        // preserve paint-property identity without narrowing the attribute's observer/epoch effects.
        if (kind == DOM_JS_MUTATION_ATTRIBUTE && attribute_name &&
            !strcmp(attribute_name, "stroke-dashoffset") && target &&
            target->is_element() && dom_element_is_svg(target->as_element()))
            record->presentation_property = CSS_PROPERTY_STROKE_DASHOFFSET;
        record->was_connected =
            dom_shadow_including_contains(doc->root, target) ||
            dom_shadow_including_contains(doc->root, parent);
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

void radiant_dom_record_mutation_detail(DomDocument* doc, DomJsMutationKind kind,
                                        DomNode* target, DomNode* parent, uint32_t sequence,
                                        const char* attribute_name, CssPropertyCode presentation_property) {
    dom_record_mutation_detail(doc, kind, target, parent, sequence,
        attribute_name, presentation_property, {});
}

void radiant_dom_mutation_begin(DomDocument* doc, DomJsMutationKind kind,
                                DomNode* target, DomNode* parent, const char* attribute_name,
                                CssPropertyCode presentation_property, const char* old_value,
                                size_t old_value_length) {
    if (!doc) return;
    parent = dom_source_inclusive_ancestor(parent);
    doc->js.mutation_count++;
    doc->js.mutation_sequence++;
    doc->advance_mutation_epoch();
    bool inherited_change = presentation_property == CSS_PROPERTY_UNKNOWN ||
        css_property_runtime_inherited(presentation_property);
    if (inherited_change) doc->style_content_epoch++;
    dom_layer_note_mutation(target, !inherited_change);
    if (parent && (!target || parent != target->parent)) dom_layer_note_mutation(parent);
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
        StrView old = {old_value, !old_value ? 0 : old_value_length == SIZE_MAX
            ? strlen(old_value) : old_value_length};
        dom_record_mutation_detail(doc, kind, target, parent, doc->js.mutation_sequence,
                                           attribute_name, presentation_property, old);
    }
}

void radiant_dom_mutation_begin(DomDocument* doc, DomJsMutationKind kind,
                                DomNode* target, DomNode* parent, const char* attribute_name,
                                CssPropertyCode presentation_property) {
    radiant_dom_mutation_begin(doc, kind, target, parent, attribute_name, presentation_property, nullptr);
}

void radiant_dom_mutation_finish(DomDocument* doc, DomJsMutationKind kind) {
    DocState* state = doc ? doc->state : nullptr;
    if (!state) return;
    view_state_prune_orphans(state);
    // publish blur before reflow validates the formerly focused area.
    View* focused = focus_get(state);
    if (focused && !radiant_event_host_focusable(focused, true) &&
        !radiant_event_host_change_focus(doc, nullptr)) {
        focus_clear(state);
    }
    // Paint-only mutations still need a style turn in a transient CLI document.
    if (kind != DOM_JS_MUTATION_STYLE_REPAINT ||
        (doc->js.host_ui_context && !doc->js.host_driven_loop)) {
        doc_state_request_reflow(state);
    }
}

void radiant_dom_notify_native(DomDocument* doc, DomJsMutationKind kind,
                               DomNode* target, DomNode* parent,
                               const char* attribute_name, const char* old_value,
                               size_t old_value_length) {
    if (!doc) doc = dom_node_ancestor_document(target);
    if (!doc) doc = dom_node_ancestor_document(parent);
    if (!doc || dom_edit_transaction_defer_mutation(doc, kind, target, parent,
                                                     attribute_name, old_value, old_value_length)) return;
    radiant_dom_mutation_begin(doc, kind, target, parent, attribute_name,
                               CSS_PROPERTY_UNKNOWN, old_value, old_value_length);
    radiant_dom_mutation_finish(doc, kind);
}

