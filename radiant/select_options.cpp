#include "select_options.hpp"
#include "lambda/core/well_known_markup_names.h"
#include "lambda/input/css/dom_element.hpp"
#include <stdlib.h>

static NameId select_tree_tag(DomElement* element) {
    NameId tag = element ? element->tag() : 0;
    switch (tag) {
        case MARKUP_NAME_SELECT: case MARKUP_NAME_OPTGROUP: case MARKUP_NAME_OPTION:
        case MARKUP_NAME_DATALIST: case MARKUP_NAME_HR: case MARKUP_NAME_TEMPLATE:
            return dom_element_is_html_tag(element, tag) ? tag : 0;
        default: return 0;
    }
}

static DomElement* select_option_owner(DomElement* option, DomElement** group) {
    DomElement* optgroup = nullptr;
    for (DomNode* ancestor = option ? option->parent.get() : nullptr;
         ancestor; ancestor = ancestor->parent) {
        DomElement* element = ancestor->as_element();
        switch (select_tree_tag(element)) {
            case MARKUP_NAME_SELECT:
                if (group) *group = optgroup;
                return element;
            case MARKUP_NAME_OPTGROUP:
                if (optgroup) return nullptr;
                optgroup = element;
                break;
            case MARKUP_NAME_OPTION: case MARKUP_NAME_DATALIST: case MARKUP_NAME_HR:
            case MARKUP_NAME_TEMPLATE:
                return nullptr;
            default:
                break;
        }
    }
    return nullptr;
}

DomElement* dom_select_option_owner(DomElement* option) {
    return select_option_owner(option, nullptr);
}

DomElement* dom_select_next_option(DomElement* select, DomElement* previous) {
    if (!select) return nullptr;
    DomElement* group = nullptr;
    if (previous && select_option_owner(previous, &group) != select) return nullptr;
    DomNode* group_end = group ? dom_next_after_subtree(select, group) : nullptr;
    DomNode* node = previous ? dom_next_after_subtree(select, previous) : select->first_child.get();
    while (node) {
        if (node == group_end) group = nullptr;
        DomElement* element = node->as_element();
        NameId tag = select_tree_tag(element);
        if (tag == MARKUP_NAME_OPTION) return element;
        // Wrappers are transparent. These HTML elements terminate the option search.
        bool skip = tag == MARKUP_NAME_SELECT || tag == MARKUP_NAME_DATALIST ||
                    tag == MARKUP_NAME_HR || tag == MARKUP_NAME_TEMPLATE;
        if (tag == MARKUP_NAME_OPTGROUP) {
            skip = group != nullptr;
            if (!group) {
                group = element;
                group_end = dom_next_after_subtree(select, group);
            }
        }
        node = element && !skip && element->first_child ? element->first_child.get()
            : dom_next_after_subtree(select, node);
    }
    return nullptr;
}

void radiant_select_walk_options(DomElement* select, RadiantSelectOptionVisitor visitor, void* context) {
    if (!visitor) return;
    for (DomElement* option = dom_select_next_option(select, nullptr); option;
         option = dom_select_next_option(select, option)) {
        visitor(option, context);
    }
}

struct SelectResetState {
    const RadiantSelectResetOps* operations;
    int count;
    int last_selected;
    int first_enabled;
    int chosen;
    bool require_one;
};

static void inspect_option(DomElement* option, void* context) {
    SelectResetState* state = static_cast<SelectResetState*>(context);
    bool selected = state->operations && state->operations->selected
        ? state->operations->selected(option, state->operations->context)
        : dom_option_is_selected(option);
    if (selected) state->last_selected = state->count;
    if (state->first_enabled < 0 && !option->has_attribute("disabled"))
        state->first_enabled = state->count;
    state->count++;
}

static void reset_option(DomElement* option, void* context) {
    SelectResetState* state = static_cast<SelectResetState*>(context);
    bool selected = state->count == (state->require_one ? state->chosen : state->last_selected);
    if (state->operations && state->operations->set_selected)
        state->operations->set_selected(option, selected, state->operations->context);
    else
        option->set_option_selectedness(selected);
    state->count++;
}

bool radiant_select_reset(DomElement* select, const RadiantSelectResetOps* operations,
                          int* selected_index, int* option_count) {
    if (!select || select->has_attribute("multiple")) return false;
    if (operations && (!operations->selected || !operations->set_selected)) return false;
    SelectResetState state = {operations, 0, -1, -1, -1, false};
    radiant_select_walk_options(select, inspect_option, &state);
    if (!state.count) return false;
    int count = state.count;
    state.chosen = state.last_selected >= 0 ? state.last_selected :
        state.first_enabled >= 0 ? state.first_enabled : 0;
    const char* size_attribute = select->get_attribute("size");
    int size = 0;
    if (size_attribute) {
        char* end = nullptr;
        long parsed = strtol(size_attribute, &end, 10);
        if (end != size_attribute && parsed > 0)
            size = (int)parsed; // INT_CAST_OK: HTML select option count
    }
    state.require_one = size <= 1;
    state.count = 0;
    radiant_select_walk_options(select, reset_option, &state);
    if (selected_index) *selected_index = state.require_one ? state.chosen : state.last_selected;
    if (option_count) *option_count = count;
    return true;
}
