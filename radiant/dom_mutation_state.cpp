#include "dom_mutation_state.hpp"
#include "event.hpp"
#include "view.hpp"

void radiant_dom_pre_remove_state(DocState* state, DomNode* child) {
    if (!state || !child) return;
    dom_mutation_pre_remove(state, child);
    View* focused = focus_get(state);
    if (focused && view_geometry_dom_is_descendant(focused, child, true)) {
        // The range adjustment already moved Selection; blur must preserve it.
        focus_clear_preserve_selection(state);
        return;
    }
    View* caret = caret_get_view(state);
    if (caret && view_geometry_dom_is_descendant(caret, child, true)) {
        state_store_caret_clear(state);
    }
    View* anchor = nullptr;
    View* focus = nullptr;
    if (selection_get_extent_views(state, &anchor, &focus) &&
        ((anchor && view_geometry_dom_is_descendant(anchor, child, true)) ||
         (focus && view_geometry_dom_is_descendant(focus, child, true)))) {
        state_store_selection_clear(state);
    }
}
