#include "dom_mutation_prepare.hpp"
#include "layout.hpp"
#include "view.hpp"
#include "../lambda/input/css/dom_element.hpp"

static DomNode* significant_sibling(DomNode* child, bool forward) {
    for (DomNode* sibling = child ? (forward ? child->next_sibling : child->prev_sibling) : nullptr;
         sibling; sibling = forward ? sibling->next_sibling : sibling->prev_sibling) {
        if (sibling->is_text()) {
            if (!layout_dom_text_has_non_whitespace(sibling->as_text())) continue;
            return sibling;
        }
        if (!sibling->is_element()) return sibling;
        if (layout_display_is_none(resolve_display_value(sibling))) continue;
        return sibling;
    }
    return nullptr;
}

static bool removal_merges_table_fixups(DomElement* parent, DomNode* child) {
    if (!parent || !child) return false;
    DomNode* previous = significant_sibling(child, false);
    DomNode* next = significant_sibling(child, true);
    // Removing a separator can join table-internal runs under generated boxes.
    return previous && previous->is_element() && previous->as_element()->is_table_fixup() &&
        next && next->is_element() && next->as_element()->is_table_fixup();
}

static bool child_is_nested_in_table_fixup(DomElement* parent, DomNode* child) {
    for (DomNode* ancestor = child ? child->parent : nullptr;
         ancestor && ancestor != static_cast<DomNode*>(parent); ancestor = ancestor->parent) {
        if (ancestor->is_element() && ancestor->as_element()->is_table_fixup()) return true;
    }
    return false;
}

DomElement* radiant_dom_prepare_children_mutation(DomElement* parent, DomNode* changed_child) {
    DomNode* source_parent = dom_source_inclusive_ancestor(parent);
    if (!source_parent || !source_parent->is_element()) return nullptr;
    DomElement* source_element = source_parent->as_element();
    bool has_table_fixup = false;
    for (DomNode* child = source_element->first_child; child; child = child->next_sibling) {
        if (child->is_element() && child->as_element()->is_table_fixup()) {
            has_table_fixup = true;
            break;
        }
    }
    // Display resolution only selects which existing anonymous boxes to unwrap.
    if (!has_table_fixup) return source_element;
    bool changes_table_structure = !changed_child || !changed_child->is_element();
    bool merges_table_fixups = false;
    bool nested = child_is_nested_in_table_fixup(source_element, changed_child);
    if (changed_child && changed_child->is_element()) {
        DisplayValue display = resolve_display_value(changed_child);
        changes_table_structure = is_table_internal_display(display.inner);
        merges_table_fixups = removal_merges_table_fixups(source_element, changed_child);
    }
    if (changes_table_structure || merges_table_fixups || nested) {
        // Unwrap layout-only boxes before editing the authored sibling chain.
        layout_unwrap_anonymous_table_fixups_for_dom_mutation(source_element);
    }
    return source_element;
}
