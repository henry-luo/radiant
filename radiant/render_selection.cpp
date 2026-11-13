#include "render.hpp"
#include "event.hpp"

bool render_selection_contains_view(DocState* state, View* view) {
    if (!state || !view) return false;

    View* anchor_view = NULL;
    View* focus_view = NULL;
    if (!selection_get_extent_views(state, &anchor_view, &focus_view)) {
        return false;
    }

    if (!anchor_view || !focus_view || anchor_view == focus_view) {
        return false;
    }

    int anchor_vs_focus = radiant_stack_compare_tree_order(anchor_view, focus_view);
    View* first_view = (anchor_vs_focus <= 0) ? anchor_view : focus_view;
    View* last_view = (anchor_vs_focus <= 0) ? focus_view : anchor_view;

    int view_vs_first = radiant_stack_compare_tree_order(view, first_view);
    int view_vs_last = radiant_stack_compare_tree_order(view, last_view);
    return view_vs_first >= 0 && view_vs_last <= 0;
}
