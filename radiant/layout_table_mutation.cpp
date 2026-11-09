#include "layout_table_mutation.hpp"
#include "layout.hpp"
#include "view.hpp"
#include "lib/arraylist.h"
#include "tagged.hpp"

void layout_table_append_detached_node(DomElement* parent, DomNode* child) {
    if (!parent || !child) return;
    child->parent = lam::up(parent);
    child->next_sibling = nullptr;
    child->prev_sibling = parent->last_child;
    if (parent->last_child) {
        parent->last_child->next_sibling = lam::own(child);
    } else {
        parent->first_child = lam::own(child);
    }
    parent->last_child = lam::up(child);
}

void layout_detach_layout_only_node(DomNode* node) {
    if (!node) return;
    DomElement* old_parent = lam::dom_as<DOM_NODE_ELEMENT>(node->parent);
    if (old_parent) {
        if (node->prev_sibling) {
            node->prev_sibling->next_sibling = node->next_sibling;
        } else {
            old_parent->first_child = node->next_sibling;
        }
        if (node->next_sibling) {
            node->next_sibling->prev_sibling = node->prev_sibling;
        } else {
            old_parent->last_child = node->prev_sibling;
        }
    }
    node->parent = nullptr;
    node->next_sibling = nullptr;
    node->prev_sibling = nullptr;
}

void layout_table_insert_node_before(DomElement* parent, DomNode* new_node, DomNode* ref_node) {
    if (!parent || !new_node) return;
    if (!ref_node) {
        layout_table_append_detached_node(parent, new_node);
        return;
    }
    new_node->parent = lam::up(parent);
    new_node->next_sibling = lam::own(ref_node);
    new_node->prev_sibling = ref_node->prev_sibling;
    if (ref_node->prev_sibling) {
        ref_node->prev_sibling->next_sibling = lam::own(new_node);
    } else {
        parent->first_child = lam::own(new_node);
    }
    ref_node->prev_sibling = lam::up(new_node);
}

static void reparent_node_before(DomNode* node, DomElement* new_parent,
                                 DomNode* before) {
    if (!node || !new_parent) return;
    layout_detach_layout_only_node(node);
    layout_table_insert_node_before(new_parent, node, before);
}

ArrayList* layout_table_snapshot_children(DomElement* parent) {
    ArrayList* children = arraylist_new(8);
    if (!children || !parent) return children;
    for (DomNode* child = parent->first_child; child; child = child->next_sibling) {
        arraylist_append(children, child);
    }
    return children;
}

void layout_table_unwrap_fixup(DomElement* fixup, DomElement* parent, DomNode* before) {
    if (!fixup || !parent) return;
    ArrayList* children = layout_table_snapshot_children(fixup);
    if (!children) return;
    for (int i = 0; i < children->length; i++) {
        DomNode* child = static_cast<DomNode*>(children->data[i]);
        if (child && child->is_element() && child->as_element()->is_table_fixup()) {
            layout_table_unwrap_fixup(child->as_element(), parent, before);
        } else if (child) {
            reparent_node_before(child, parent, before);
        }
    }
    arraylist_free(children);
    layout_detach_layout_only_node(static_cast<DomNode*>(fixup));
    // Fixup nodes are view-pool owned. Once their authored children have been
    // restored, retire the empty wrapper in the same layout ownership epoch.
    if (fixup->doc && fixup->doc->view_tree) {
        view_tree_release_retired_subtree(
            fixup->doc->view_tree, static_cast<DomNode*>(fixup));
    }
}

void layout_unwrap_anonymous_table_fixups_for_dom_mutation(DomElement* parent) {
    if (!parent) return;
    for (DomNode* child = parent->first_child; child; ) {
        DomNode* next = child->next_sibling;
        if (child->is_element() && child->as_element()->is_table_fixup()) {
            DomElement* fixup = child->as_element();
            // Anonymous table boxes are layout-only and cannot become DOM
            // mutation state; reflow must recompute current source runs rather
            // than retain partitions made while a cell was display:none.
            layout_table_unwrap_fixup(fixup, parent, next);
        }
        child = next;
    }
}

void layout_unwrap_all_anonymous_table_fixups_for_dom_mutation(DomElement* root) {
    if (!root) return;
    layout_unwrap_anonymous_table_fixups_for_dom_mutation(root);
    for (DomNode* child = root->first_child; child; child = child->next_sibling) {
        if (child->is_element()) {
            layout_unwrap_all_anonymous_table_fixups_for_dom_mutation(
                lam::dom_require_element(child));
        }
    }
    if (root->shadow_root_element()) {
        layout_unwrap_all_anonymous_table_fixups_for_dom_mutation(
            root->shadow_root_element());
    }
}

