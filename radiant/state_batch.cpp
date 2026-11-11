#include "state_store_internal.hpp"

void state_begin_batch(DocState* state) {
    if (!state) return;
    state->state_batch_depth++;
}

void state_sync_selection_before_assert(DocState* state) {
    if (!state) return;
    selection_refresh_presentation(state);
}

static void state_sync_editing_before_assert(DocState* state) {
    if (!state) return;
    // Script-owned editors can move focus while an input transaction is still
    // open; refresh the projection before publishing the batch so the active
    // surface cannot remain attached to the block that just lost focus.
    editing_interaction_sync_projection(state);
}

void state_sync_dirty_flags_before_assert(DocState* state) {
    if (!state) return;

    // Dirty tracking is authoritative for pending visual work. Keep flags
    // aligned so invariant checks and the render loop observe one source.
    if (state->dirty_tracker.full_reflow) {
        state->needs_reflow = true;
    }
    if (state->selection_layout_dirty || state->dirty_tracker.full_repaint ||
        state->dirty_tracker.full_reflow || dirty_has_regions(&state->dirty_tracker)) {
        state->needs_repaint = true;
    }
}

void state_end_batch(DocState* state) {
    if (!state || state->state_batch_depth == 0) return;
    state->state_batch_depth--;
    if (state->state_batch_depth > 0) return;
    state_sync_selection_before_assert(state);
    state_sync_editing_before_assert(state);
    state_sync_dirty_flags_before_assert(state);
    // TODO: trigger deferred callbacks
    // event cascades validate the synchronized state at their own boundary.
    if (state->active_cascade_depth == 0)
        radiant_state_assert_valid(state, "state_end_batch");
}

