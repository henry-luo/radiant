#include "select_options.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lib/str.h"
#include <stdlib.h>

static void walk_options(DomNode* node, bool allow_optgroup,
                         RadiantSelectOptionVisitor visitor, void* context) {
    while (node) {
        if (node->is_element()) {
            DomElement* element = node->as_element();
            const char* tag = element->tag_name;
            if (tag && str_icmp_cstr(tag, "option") == 0) {
                visitor(element, context);
            } else if (tag && str_icmp_cstr(tag, "select") != 0 &&
                       str_icmp_cstr(tag, "hr") != 0) {
                bool optgroup = str_icmp_cstr(tag, "optgroup") == 0;
                if (!optgroup || allow_optgroup)
                    walk_options(element->first_child, allow_optgroup && !optgroup,
                                 visitor, context);
            }
        }
        node = node->next_sibling;
    }
}

void radiant_select_walk_options(DomNode* first, RadiantSelectOptionVisitor visitor, void* context) {
    if (visitor) walk_options(first, true, visitor, context);
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
    radiant_select_walk_options(select->first_child, inspect_option, &state);
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
    radiant_select_walk_options(select->first_child, reset_option, &state);
    if (selected_index) *selected_index = state.require_one ? state.chosen : state.last_selected;
    if (option_count) *option_count = count;
    return true;
}
