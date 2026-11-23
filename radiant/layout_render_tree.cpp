#include "layout.hpp"
#include "css_resolved_style.hpp"
#include "tagged.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"
#include <string.h>

// Render-tree traversal is shared by layout, painting, and isolated host tests.
DomElement* layout_shadow_formatting_parent(DomNode* node) {
    if (!node) return nullptr;
    if (node->parent && node->parent->is_element()) {
        DomElement* parent = node->parent->as_element();
        if (parent->tag_name() && strcmp(parent->tag_name(), "#document-fragment") == 0) {
            return parent->shadow_host_element();
        }
    }
    for (DomNode* ancestor = node->parent; ancestor; ancestor = ancestor->parent) {
        if (!ancestor->is_element()) continue;
        DomElement* element = ancestor->as_element();
        DomElement* shadow_root = element->shadow_root_element();
        if (shadow_root) {
            for (DomNode* child = shadow_root->first_child; child;
                 child = child->next_sibling) {
                if (child == node) return element;
            }
        }
    }
    return nullptr;
}

static DomElement* layout_shadow_tree_host(DomNode* node) {
    DomElement* formatting_parent = layout_shadow_formatting_parent(node);
    if (formatting_parent) return formatting_parent;
    for (DomNode* ancestor = node ? node->parent : nullptr;
         ancestor; ancestor = ancestor->parent) {
        if (!ancestor->is_element()) continue;
        DomElement* element = ancestor->as_element();
        if (element->tag_name() && strcmp(element->tag_name(), "#document-fragment") == 0) {
            return element->shadow_host_element();
        }
        if (element->shadow_root_element() &&
            dom_subtree_contains_node(element->shadow_root_element(), node)) {
            return element;
        }
    }
    return nullptr;
}

DomNode* layout_render_child_list(DomElement* element) {
    if (!element) return nullptr;
    DomElement* shadow_root = element->shadow_root_element();
    // A shadow root replaces the host's rendered child tree; its light-DOM
    // children remain available for slot assignment and DOM APIs.
    return shadow_root ? shadow_root->first_child : element->first_child;
}

DomNode* layout_rendered_first_child_node(DomElement* element) {
    DomNode* child = layout_paint_first_child(element);
    while (child && !child->view_type) child = layout_paint_next(child);
    return child;
}

bool layout_is_shadow_slot(DomNode* node) {
    return node && node->is_element() && dom_element_is_html_tag(node->as_element(), MARKUP_NAME_SLOT) &&
        layout_shadow_tree_host(node) != nullptr;
}

bool layout_slot_is_hidden(DomNode* slot) {
    return layout_resolved_display(slot).outer == CSS_VALUE_NONE;
}

static DomNode* layout_source_sibling(DomNode* node, bool forward) {
    return forward ? node->next_sibling.get() : node->prev_sibling.get();
}

static DomElement* layout_projection_slot(DomNode* node) {
    DomElement* parent = node->parent ? node->parent->as_element() : nullptr;
    return parent && parent->shadow_root_element() ? dom_find_assigned_slot(node) : nullptr;
}

ViewElement* layout_render_parent(DomNode* node) {
    if (!node) return nullptr;
    // paint and visual geometry must stop at the top-layer boundary.
    if (layout_is_top_layer_box(node->as_element())) return nullptr;
    DomNode* parent = layout_projection_slot(node);
    if (!parent) parent = node->parent;
    while (layout_is_shadow_slot(parent)) {
        DomNode* slot = parent;
        parent = layout_projection_slot(slot);
        if (!parent) parent = slot->parent;
    }
    DomElement* element = parent ? parent->as_element() : nullptr;
    // Slot and shadow fragment nodes have no formatting box.
    return lam::view_as_element(element && element->shadow_host_element()
        ? element->shadow_host_element() : element);
}

static DomNode* layout_paint_source_sibling(DomNode* node, bool forward) {
    while (node) {
        if (DomElement* slot = layout_projection_slot(node)) {
            if (DomNode* assigned = dom_slot_assigned_adjacent(dom_slot_assignment(slot), node, forward))
                return assigned;
            node = slot;
        } else {
            DomNode* sibling = layout_source_sibling(node, forward);
            if (sibling) return sibling;
            node = node->parent;
            if (!layout_is_shadow_slot(node)) return nullptr;
        }
    }
    return nullptr;
}

static DomNode* layout_paint_expand_slot(DomNode* node, bool forward) {
    while (node) {
        if (layout_is_backdrop(node->as_element())) {
            node = layout_paint_source_sibling(node, forward);
            continue;
        }
        if (!layout_is_shadow_slot(node)) break;
        DomElement* slot = node->as_element();
        DomNode* first = nullptr;
        // display:none suppresses both assigned and fallback descendants.
        if (!layout_slot_is_hidden(slot)) {
            first = dom_slot_assigned_adjacent(dom_slot_assignment(slot), nullptr, forward);
            if (!first) first = forward ? slot->first_child.get() : slot->last_child.get();
        }
        node = first ? first : layout_paint_source_sibling(node, forward);
    }
    return node;
}

DomNode* layout_paint_first_child(DomElement* element) {
    return layout_paint_expand_slot(layout_render_child_list(element), true);
}

DomNode* layout_paint_last_child(DomElement* element) {
    if (!element) return nullptr;
    DomElement* root = element->shadow_root_element();
    return layout_paint_expand_slot(root ? root->last_child.get() : element->last_child.get(), false);
}

DomNode* layout_paint_next(DomNode* node) {
    return layout_paint_expand_slot(layout_paint_source_sibling(node, true), true);
}

DomNode* layout_paint_previous(DomNode* node) {
    return layout_paint_expand_slot(layout_paint_source_sibling(node, false), false);
}

bool layout_shadow_suppresses_box(DomElement* element) {
    if (!element || !element->doc || !element->doc->js.has_shadow_roots) return false;
    if (layout_is_shadow_slot(element)) return true;
    // A source ancestor may be unassigned or suppressed fallback. Retained
    // geometry on that subtree must not appear in client rectangles.
    for (DomElement* node = element; node;) {
        if (layout_is_shadow_slot(node) && layout_slot_is_hidden(node)) return true;
        DomElement* parent = dom_flat_tree_parent(node);
        if (!parent) return dom_source_parent(node) != nullptr;
        node = parent;
    }
    return false;
}

DomElement* layout_top_layer_backdrop(DomElement* owner) {
    for (DomNode* node = owner ? owner->last_child.get() : nullptr; node; node = node->prev_sibling) {
        DomElement* element = node->as_element();
        if (layout_is_backdrop(element)) return element;
    }
    return nullptr;
}

