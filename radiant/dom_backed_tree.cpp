#include "dom_backed_tree.hpp"
#include "dom_mutation_prepare.hpp"
#include "dom_mutation_record.hpp"
#include "dom_mutation_state.hpp"
#include "editing_dom_internal.hpp"
#include "select_options.hpp"
#include "event.hpp"
#include "render.hpp"
#include "../lambda/input/css/dom_element.hpp"
#include "../lambda/input/css/dom_lifecycle.hpp"
#include "../lib/str.h"

static bool native_remove(DomElement* parent, DomNode* child) {
    parent = radiant_dom_prepare_children_mutation(parent, child);
    return dom_remove_prepared_backed_child(parent, child);
}

static const RadiantDomBackedTreeOps native_operations = {
    radiant_dom_prepare_children_mutation, native_remove,
};

bool radiant_dom_insert_backed_element(DomElement* parent, DomNode* child,
                                       DomNode* ref_child, const RadiantDomBackedTreeOps* operations) {
    const RadiantDomBackedTreeOps* ops = operations ? operations : &native_operations;
    if (!ops->prepare || !ops->remove) return false;
    if (!parent || !child || !child->is_element()) return false;
    parent = ops->prepare(parent, nullptr);
    if (!parent) return false;
    if (ref_child && ref_child->parent != (DomNode*)parent) return false;
    DomElement* child_elem = child->as_element();
    if (parent->is_synthetic() || child_elem->is_synthetic()) {
        if (child->parent && !child->parent->remove_child(child)) return false;
        return ((DomNode*)parent)->insert_before(child, ref_child);
    }
    if (!parent->doc || !parent->doc->input) return false;
    if (child == ref_child ||
        (child->parent == (DomNode*)parent && child->next_sibling == ref_child)) {
        return true;
    }

    if (child->parent) {
        if (!child->parent->is_element() ||
            !ops->remove(child->parent->as_element(), child)) {
            return false;
        }
    } else {
        int64_t backed_index = dom_backed_node_index(parent, child_elem);
        if (backed_index >= 0 &&
            !dom_remove_backed_element_item(parent, child_elem, backed_index)) {
            return false;
        }
    }

    return dom_insert_detached_backed_element(parent, child_elem, ref_child);
}

bool radiant_dom_insert_backed_text(DomElement* parent, DomText* text,
                                    DomNode* ref_child, const RadiantDomBackedTreeOps* operations) {
    const RadiantDomBackedTreeOps* ops = operations ? operations : &native_operations;
    if (!ops->prepare || !ops->remove) return false;
    if (!parent || !text || !text->native_string || !parent->doc || !parent->doc->input) return false;
    if (!parent->is_synthetic()) {
        parent = ops->prepare(parent, nullptr);
        if (!parent) return false;
    }
    return dom_insert_prepared_backed_text(parent, text, ref_child);
}

static void native_pre_remove_for_move(DomElement* parent, DomNode* child) {
    if (!parent || !child || !child->parent) return;
    DomNode* old_parent = child->parent;
    DomDocument* document = dom_node_ancestor_document(old_parent);
    radiant_dom_pre_remove_state(document ? document->state : nullptr, child);
    if (!document || document != parent->doc) view_pool_release_detached_subtree(child);
    radiant_dom_record_mutation_detail(document, DOM_JS_MUTATION_CHILD_REMOVE, child,
                                       old_parent, 0, nullptr, CSS_PROPERTY_UNKNOWN);
}

static void native_post_insert(DomNode* parent, DomNode* child) {
    DomDocument* document = dom_node_ancestor_document(child);
    if (!document) document = dom_node_ancestor_document(parent);
    if (!document) return;
    if (document->state) dom_mutation_post_insert(document->state, parent, child);
    dom_document_process_metadata_insertion(document, child);
    radiant_dom_record_mutation_detail(document, DOM_JS_MUTATION_CHILD_INSERT, child,
                                       parent, 0, nullptr, CSS_PROPERTY_UNKNOWN);
}

static void native_notify_insert(DomNode* parent, DomNode* child) {
    radiant_dom_notify_native(nullptr, DOM_JS_MUTATION_CHILD_INSERT, child, parent, nullptr, nullptr);
}

static const RadiantDomTreeMutationOps native_mutation_operations = {
    {radiant_dom_prepare_children_mutation, native_remove},
    native_pre_remove_for_move, native_post_insert, native_notify_insert,
};

bool radiant_dom_insert_before_child(DomElement* parent, DomNode* child, DomNode* reference,
                                     const RadiantDomTreeMutationOps* operations) {
    const RadiantDomTreeMutationOps* ops = operations ? operations : &native_mutation_operations;
    if (!parent || !child || !ops->pre_remove_for_move || !ops->post_insert || !ops->notify_insert) return false;
    if (!operations) {
        DomDocument* owner = dom_node_ancestor_document(child);
        // Native editing cannot silently adopt a node from another document.
        if (!parent->doc || (reference && reference->parent != (DomNode*)parent) ||
            (owner && owner != parent->doc) ||
            !dom_node_registry_owns(parent->doc, child)) return false;
    }
    if (child == reference ||
        (child->parent == (DomNode*)parent && child->next_sibling == reference)) return true;
    ops->pre_remove_for_move(parent, child);
    bool inserted = child->is_element()
        ? radiant_dom_insert_backed_element(parent, child, reference, &ops->backing)
        : child->is_text()
            ? radiant_dom_insert_backed_text(parent, child->as_text(), reference, &ops->backing)
            : ((child->parent ? child->parent->remove_child(child) : true) &&
               ((DomNode*)parent)->insert_before(child, reference));
    if (!inserted) return false;
    ops->post_insert(parent, child);
    ops->notify_insert(parent, child);
    return true;
}

bool radiant_dom_remove_edit_child(DomNode* child) {
    if (!child || !child->parent || !child->parent->is_element()) return false;
    DomElement* parent = child->parent->as_element();
    DomDocument* document = parent->doc;
    if (!document || !dom_node_registry_owns(document, child)) return false;
    bool reset_select = parent->tag_name && str_icmp_cstr(parent->tag_name, "select") == 0 &&
        child->is_element() && child->as_element()->tag_name &&
        str_icmp_cstr(child->as_element()->tag_name, "option") == 0;
    if (reset_select && !dom_edit_snapshot_native_select(document, parent)) return false;
    bool deferred = false;
    if (!dom_edit_defer_native_retirement(document, child, &deferred) || !deferred) return false;
    DomNode* old_parent = child->parent;
    native_pre_remove_for_move(parent, child);
    if (!native_remove(parent, child)) return false;
    if (reset_select) {
        int selected_index = -1, option_count = 0;
        if (radiant_select_reset(parent, nullptr, &selected_index, &option_count))
            radiant_select_sync_native_index(parent, selected_index, option_count);
    }
    radiant_dom_notify_native(document, DOM_JS_MUTATION_CHILD_REMOVE, child, old_parent, nullptr, nullptr);
    return true;
}

