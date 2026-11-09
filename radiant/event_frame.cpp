#include "event_frame.hpp"
#include "event.hpp"
#include "event_host.hpp"
#include "layout.hpp"
#include "render.hpp"
#include "radiant.hpp"
#include "tagged.hpp"
#include "lib/log.h"

static void restore_embedded_document_scroll_model(DomDocument* doc) {
    if (!doc || !doc->view_tree || !doc->view_tree->root ||
        doc->view_tree->root->view_type != RDT_VIEW_BLOCK) {
        return;
    }

    ViewBlock* root = lam::view_require_block(doc->view_tree->root);
    if (!root->scroller) return;

    if (root->content_height > root->height) {
        root->height = root->content_height;
    }
    root->scroller = NULL;
}

void layout_event_document_reflow(EventContext* evcon, DomDocument* doc,
                                         View* iframe_container) {
    if (!evcon || !evcon->ui_context || !doc) return;

    UiContext* uicon = evcon->ui_context;
    DomDocument* saved_doc = uicon->document;
    float saved_viewport_width = uicon->viewport_width;
    float saved_viewport_height = uicon->viewport_height;

    uicon->document = lam::up(doc);

    if (iframe_container &&
        (iframe_container->view_type == RDT_VIEW_BLOCK ||
         iframe_container->view_type == RDT_VIEW_INLINE_BLOCK)) {
        ViewBlock* block = lam::view_require_block(iframe_container);
        if (block->width > 0) {
            uicon->viewport_width = (int)block->width; // INT_CAST_OK: UiContext viewport dimensions are integer CSS pixels.
        }
        if (block->height > 0) {
            uicon->viewport_height = (int)block->height; // INT_CAST_OK: UiContext viewport dimensions are integer CSS pixels.
        }
    }

    layout_update_document(uicon, doc);
    if (iframe_container) {
        restore_embedded_document_scroll_model(doc);
    }

    uicon->document = lam::up(saved_doc);
    uicon->viewport_width = (int)saved_viewport_width; // INT_CAST_OK: restore the integer viewport API.
    uicon->viewport_height = (int)saved_viewport_height; // INT_CAST_OK: restore the integer viewport API.
}

struct DocumentAnimationTick {
    double now;
    int depth;
    bool tick_root;
    bool anchor_host_time;
    bool ticked;
    bool active;
};

bool radiant_tick_document_animation_scheduler(DomDocument* document, double now,
                                               DirtyTracker* dirty_tracker) {
    DocState* state = document ? document->state : nullptr;
    AnimationScheduler* scheduler = state ? state->animation_scheduler : nullptr;
    if (!scheduler || !scheduler->has_active_animations) return false;
    bool active = animation_scheduler_tick(scheduler, now, dirty_tracker);
    // sampled sizes must reach geometry before native or virtual frames paint.
    if (scheduler->needs_layout) doc_state_request_reflow(state);
    doc_state_request_repaint(state);
    return active;
}

static void tick_document_animation_tree(DomDocument* document,
                                         DocumentAnimationTick* tick);

static bool tick_embedded_document_animation(View* view, void* user_data) {
    DocumentAnimationTick* tick = (DocumentAnimationTick*)user_data;
    if (!view || !view->is_block() || !tick) return true;
    ViewBlock* block = lam::view_require_block(view);
    DomDocument* embedded = block->embed ? block->embedp()->doc : nullptr;
    if (embedded) {
        if (tick->depth < MAX_IFRAME_DEPTH && embedded->js.host_ui_context &&
            embedded->js.mutation_count > 0 &&
            !layout_is_active(embedded) && radiant_event_host_allow_frame_reflow()) {
            // a parent callback can mutate its child's CSS; commit it at the same frame boundary.
            EventContext event = {};
            event.ui_context = (UiContext*)embedded->js.host_ui_context;
            layout_event_document_reflow(&event, embedded, block);
            tick->ticked = true;
        }
        DocumentAnimationTick child = {
            tick->now, tick->depth + 1, true, tick->anchor_host_time, false, false
        };
        tick_document_animation_tree(embedded, &child);
        tick->ticked = tick->ticked || child.ticked;
        tick->active = tick->active || child.active;
    }
    return true;
}

static void tick_document_animation_tree(DomDocument* document,
                                         DocumentAnimationTick* tick) {
    if (!document || !tick || tick->depth > MAX_IFRAME_DEPTH) return;
    DocState* state = document->state;
    AnimationScheduler* scheduler = state ? state->animation_scheduler : nullptr;
    if (scheduler && tick->anchor_host_time) {
        animation_scheduler_anchor_host_time(scheduler, tick->now);
    }
    if (scheduler && scheduler->has_active_animations &&
        (tick->depth > 0 || tick->tick_root)) {
        // Embedded bounds are local to their own viewport; the host repaints
        // the complete frame after any child tick.
        DirtyTracker* dirty = tick->depth == 0 ? &state->dirty_tracker : nullptr;
        tick->active = radiant_tick_document_animation_scheduler(
            document, tick->now, dirty) || tick->active;
        tick->ticked = true;
    }
    if (state && state->has_active_smooth_scroll &&
        (tick->depth > 0 || tick->tick_root)) {
        tick->active = scroll_smooth_tick_document(document, tick->now) ||
            tick->active;
        tick->ticked = true;
    }
    if (tick->depth > 0 || tick->tick_root) {
        tick->ticked = radiant_event_host_frame_tick(document, tick->now * 1000.0) || tick->ticked;
        tick->active = radiant_event_host_pending_frames(document) || tick->active;
    }
    if (document->view_tree && document->view_tree->root) {
        view_geometry_walk_elements(document->view_tree->root,
                                    tick_embedded_document_animation, tick);
    }
}

bool radiant_tick_document_animations(DomDocument* document, double now,
                                      bool tick_root, bool anchor_host_time) {
    DocumentAnimationTick tick = {
        now, 0, tick_root, anchor_host_time, false, false
    };
    DocState* state = document ? document->state : nullptr;
    uint64_t full_requests = state ? state->full_repaint_requests : 0;
    tick_document_animation_tree(document, &tick);
    if (tick.ticked && state) {
        // Frame callbacks whose only outcome was presentation damage keep their region
        // repaint (Radiant_Design_Animation_Repaint §3 A2). Any full request during the
        // tick (canvas, scene3d, CSS animation, a non-paint-only commit) repaints in full.
        if (state->presentation_damage.pending && state->full_repaint_requests == full_requests)
            doc_state_request_region_repaint(state);
        else doc_state_request_repaint(state);
    }
    return tick.active;
}
