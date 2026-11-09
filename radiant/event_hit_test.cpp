#include "event_hit_test.hpp"
#include "event_host.hpp"
#include "svg_geometry_context.hpp"
#include "svg_hit.hpp"
#include "layout.hpp"
#include "render.hpp"
#include "view.hpp"
#include "radiant.hpp"
#include "tagged.hpp"
#include "form_controls.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"
#include "lambda/input/css/css_engine.hpp"
#include "lib/arraylist.h"
#include "lib/font/font.h"
#include "lib/log.h"
#include <assert.h>
#include <math.h>

DocState* event_context_target_state(EventContext* evcon) {
    if (!evcon) return NULL;
    DomDocument* target_doc = evcon->target_document
        ? evcon->target_document
        : (evcon->ui_context ? evcon->ui_context->document : NULL);
    return target_doc ? target_doc->state : NULL;
}

DocState* event_view_owner_state(View* view) {
    if (!view || !view->is_element()) return NULL;
    DomElement* elem = lam::dom_require_element(view);
    return elem && elem->doc ? (DocState*)elem->doc->state : NULL;
}

static bool event_view_pointer_events_none(View* view) {
    for (DomNode* node = static_cast<DomNode*>(view); node;
         node = node->parent) {
        if (!node->is_element()) continue;
        CssEnum value = layout_specified_keyword(
            lam::dom_require_element(node), CSS_PROPERTY_POINTER_EVENTS,
            CSS_VALUE__UNDEF);
        if (value != CSS_VALUE__UNDEF) return value == CSS_VALUE_NONE;
    }
    return false;
}



static void target_stacking_view(EventContext* evcon, View* view) {
    if (!evcon || !view || evcon->target) return;
    RadiantPaintPhaseScope phase(&evcon->paint_phase,
        radiant_stack_is_paint_atomic(view) || radiant_stack_is_float(view) ||
        radiant_stack_is_out_of_flow_positioned(view) || radiant_stack_is_in_flow_positioned_step8(view)
            ? RADIANT_PAINT_ALL : evcon->paint_phase);

    if (view->is_block()) {
        target_block_view(evcon, lam::view_require_block(view));
    } else if (view->view_type == RDT_VIEW_INLINE) {
        target_inline_view(evcon, lam::view_require_element(view));
    } else if (view->view_type == RDT_VIEW_TEXT) {
        target_text_view(evcon, lam::view_require_text(view));
    }
}

static void target_stacking_list_reverse(EventContext* evcon, ArrayList* views) {
    if (!evcon || !views) return;

    // Hit-testing consumes the same stable paint-order list in reverse so equal
    // z-index siblings target the later painted node instead of drifting from render.
    for (int i = views->length - 1; i >= 0 && !evcon->target; i--) {
        target_stacking_view(evcon, (View*)views->data[i]);
    }
}



static void target_positioned_children(EventContext* evcon, ViewBlock* block) {
    ArrayList* views = radiant_stack_collect_positioned_children(
        block, "[RAD_CAP_POSITIONED_HIT]");
    if (!views) return;

    radiant_stack_sort_in_paint_order(views);
    target_stacking_list_reverse(evcon, views);
    arraylist_free(views);
}

static bool target_stacking_layer(void* context, View* owner, View* layer) {
    EventContext* evcon = (EventContext*)context;
    View* saved = evcon->stacking_path;
    evcon->stacking_path = lam::up(layer);
    target_stacking_view(evcon, radiant_stack_path_child(owner, layer));
    evcon->stacking_path = lam::up(saved);
    return !evcon->target;
}

static void target_positive_z_descendants(EventContext* evcon, View* first_child) {
    if (first_child && !evcon->target) radiant_stack_visit_positive_z_descendants(
        first_child->parent, true, evcon, target_stacking_layer);
}

static void target_negative_z_descendants(EventContext* evcon, ViewBlock* block) {
    if (!evcon->target) radiant_stack_visit_negative_z_descendants(block, true, evcon, target_stacking_layer);
}

static void target_custom_layout_children(EventContext* evcon, ViewBlock* block) {
    if (!evcon || !block || !block->custom_layout_paint_prop()) return;
    RadiantStackPaintList paint = radiant_stack_collect_custom_layout_paint(block);
    // Generated SVG layers are non-interactive, but their position in this
    // sequence determines which authored child is visually topmost.
    for (int i = paint.count - 1; i >= 0 && !evcon->target; i--) {
        if (!paint.entries[i].is_generated_layer && paint.entries[i].view) {
            target_stacking_view(evcon, paint.entries[i].view);
        }
    }
    radiant_stack_free_custom_layout_paint(&paint);
}

void target_children(EventContext* evcon, View* view) {
    if (!evcon || !view) return;
    unsigned phases = evcon->paint_phase == RADIANT_PAINT_ALL ? radiant_stack_flow_phases(view) : 0;
    if (phases) {
        // hit testing consumes the same CSS paint phases in reverse order.
        for (unsigned index = 0; index < 3; index++) {
            RadiantPaintPhase next = radiant_stack_paint_phase(2 - index);
            if (!(phases & (1u << next))) continue;
            RadiantPaintPhaseScope phase(&evcon->paint_phase, next);
            target_children(evcon, view);
            if (evcon->target) return;
        }
        return;
    }
    View* last = view;
    for (View* child = view; child; child = child->next()) last = child;
    if (render_paint_phase_has_content(evcon->paint_phase)) {
        for (View* child = last; child && !evcon->target;
             child = child == view ? nullptr : static_cast<View*>(child->prev_sibling)) {
            if (!radiant_stack_is_deferred_from_normal_flow(child) &&
                radiant_stack_is_in_flow_positioned_step8(child)) target_stacking_view(evcon, child);
        }
    }
    for (View* child = last; child && !evcon->target;
         child = child == view ? nullptr : static_cast<View*>(child->prev_sibling)) {
        if (radiant_stack_is_deferred_from_normal_flow(child) ||
            radiant_stack_is_in_flow_positioned_step8(child) ||
            !radiant_stack_in_paint_phase(child, evcon->paint_phase)) continue;
        target_stacking_view(evcon, child);
    }
}

void target_text_view(EventContext* evcon, ViewText* text) {
    if (dom_node_is_inert(text) || view_visibility_is_hidden(text) ||
        event_view_pointer_events_none(static_cast<View*>(text))) return;
    unsigned char* str = text->text_data();
    TextRect *text_rect = text->rect;
    MousePositionEvent* event = &evcon->event.mouse_position;

    NEXT_RECT:
    float x = evcon->block.x + text_rect->x, y = evcon->block.y + text_rect->y;
    float pdf_width = 0.0f;
    bool pdf_copy_space = false;
    float rect_width = text_rect->width;
    if (view_geometry_pdf_text_metrics(text, &pdf_width, &pdf_copy_space)) {
        rect_width = pdf_width;
    }
    float rect_right = x + rect_width;
    float rect_bottom = y + text_rect->height;

    log_debug("target text:'%t' start:%d, len:%d, x:%.1f, y:%.1f, wd:%.1f, hg:%.1f, blk_x:%.1f",
        str, text_rect->start_index, text_rect->length, text_rect->x, text_rect->y, text_rect->width, text_rect->height, evcon->block.x);

    // First check if mouse is within the text rect bounds (use rect height, not char height)
    if (x <= event->x && event->x < rect_right && y <= event->y && event->y < rect_bottom) {
        // Mouse is in this text rect - set target and return
        log_debug("hit on text rect at (%.1f, %.1f)", event->x, event->y);
        evcon->target = text;
        evcon->target_text_rect = text_rect;
        return;
    }

    assert(text_rect->next != text_rect);
    text_rect = text_rect->next;
    if (text_rect) { goto NEXT_RECT; }
}

typedef struct EditableMarginTextHit {
    ViewText* text;
    TextRect* rect;
    int offset;
    float block_x;
    float block_y;
    float score;
} EditableMarginTextHit;

static bool is_in_rich_editable_subtree(View* view) {
    EditingSurface surface;
    return editing_surface_from_target(view, &surface) &&
        editing_surface_is_rich(&surface);
}

bool radiant_hit_rich_editable_host(View* view) {
    if (!view || !view->is_element()) return false;
    DomElement* elem = lam::dom_require_element(view);
    EditingSurface surface;
    if (!editing_surface_from_target(view, &surface)) return false;
    return editing_surface_is_rich(&surface) && surface.owner == elem;
}

static bool event_inside_block(EventContext* evcon, ViewBlock* block) {
    if (!evcon || !block) return false;
    MousePositionEvent* event = &evcon->event.mouse_position;
    return evcon->block.x <= event->x && event->x < evcon->block.x + block->width &&
           evcon->block.y <= event->y && event->y < evcon->block.y + block->height;
}

static void find_editable_margin_text_hit(EventContext* evcon, View* view,
                                          float block_x, float block_y,
                                          EditableMarginTextHit* hit,
                                          bool include_vertical_gap) {
    if (!evcon || !view || !hit) return;

    MousePositionEvent* event = &evcon->event.mouse_position;

    if (view->view_type == RDT_VIEW_TEXT) {
        if (view_visibility_is_hidden(view)) return;
        ViewText* text = lam::view_require_text(view);
        for (TextRect* rect = text->rect; rect; rect = rect->next) {
            if (rect->height <= 0) continue;
            float rect_x = block_x + rect->x;
            float rect_y = block_y + rect->y;
            float rect_right = rect_x + rect->width;
            float rect_bottom = rect_y + rect->height;

            float score = -1.0f;
            int offset = rect->start_index;
            if (rect_y <= event->y && event->y < rect_bottom && event->x < rect_x) {
                // marker/leading-margin clicks belong to this line before vertical-gap snapping.
                score = rect_x - event->x;
            } else if (rect_y <= event->y && event->y < rect_bottom && event->x >= rect_right) {
                score = event->x - rect_right;
                offset = rect->start_index + max(rect->length, 0);
            } else if (include_vertical_gap && event->y >= rect_bottom) {
                score = (event->y - rect_bottom) + 10000.0f;
                offset = rect->start_index + max(rect->length, 0);
            } else if (include_vertical_gap && event->y < rect_y) {
                score = (rect_y - event->y) + 20000.0f;
                offset = rect->start_index;
            }

            if (score >= 0.0f && (!hit->text || score < hit->score)) {
                hit->text = text;
                hit->rect = rect;
                hit->offset = offset;
                hit->block_x = block_x;
                hit->block_y = block_y;
                hit->score = score;
            }
        }
        return;
    }

    if (!view->is_element()) return;

    float child_block_x = block_x;
    float child_block_y = block_y;
    if (view->view_type == RDT_VIEW_BLOCK ||
        view->view_type == RDT_VIEW_INLINE_BLOCK ||
        view->view_type == RDT_VIEW_LIST_ITEM) {
        child_block_x += view->x;
        child_block_y += view->y;
    }

    DomElement* elem = lam::dom_require_element(view);
    for (DomNode* child = elem->first_child; child; child = child->next_sibling) {
        View* child_view = static_cast<View*>(child);
        if (!child_view->view_type) continue;
        find_editable_margin_text_hit(evcon, child_view, child_block_x, child_block_y, hit,
                          include_vertical_gap);
    }
}

void target_inline_view(EventContext* evcon, ViewSpan* view_span) {
    if (dom_node_is_inert(view_span)) return;
    log_enter();
    FontBox pa_font = evcon->font;
    if (evcon->stacking_path == static_cast<View*>(view_span)) evcon->stacking_path = nullptr;
    View* view = view_span->first_child;
    if (view) {
        if (view_span->font) {
            setup_font(evcon->ui_context, &evcon->font, view_span->font);
        }
        if (evcon->stacking_path) target_stacking_view(evcon,
            radiant_stack_path_child(view_span, evcon->stacking_path));
        else target_children(evcon, view);
    }
    evcon->font = pa_font;
    log_leave();
}

void target_block_view(EventContext* evcon, ViewBlock* block) {
    if (dom_node_is_inert(block)) return;
    if (render_text_overflow_atomic_hidden(evcon->ui_context, block)) return;
    log_enter();
    if (view_backface_is_hidden(block, BACKFACE_HIT_TEST)) {
        log_leave();
        return;
    }
    if (block && block->tag() == MARKUP_NAME_SVG) {
        // SVG subtrees have user-space geometry; HTML boxes inside foreignObject
        // must be walked only after the SVG frame has mapped the pointer locally.
        float input_x = evcon->event.mouse_position.x, input_y = evcon->event.mouse_position.y;
        float screen_x = evcon->viewport_pointer_x, screen_y = evcon->viewport_pointer_y;
        DomElement* hit = (DomElement*)dom_svg_element_from_point(block, screen_x, screen_y);
        if (hit) {
            for (DomNode* ancestor = hit; ancestor && ancestor != static_cast<DomNode*>(block);
                 ancestor = ancestor->parent) {
                DomElement* element = ancestor->as_element();
                if (!element || element->tag() != MARKUP_NAME_FOREIGNOBJECT) continue;
                float local_x, local_y;
                ViewBlock* foreign = lam::view_as_block(element);
                if (foreign && dom_svg_foreign_object_local_point(element, screen_x, screen_y, &local_x, &local_y)) {
                    BlockBlot parent = evcon->block; evcon->block = {};
                    evcon->event.mouse_position.x = local_x; evcon->event.mouse_position.y = local_y;
                    target_block_view(evcon, foreign);
                    evcon->event.mouse_position.x = input_x; evcon->event.mouse_position.y = input_y;
                    if (!evcon->target) evcon->block = parent;
                }
                break;
            }
            if (!evcon->target) evcon->target = static_cast<View*>(hit);
        }
        log_leave();
        return;
    }
    BlockBlot pa_block = evcon->block;  FontBox pa_font = evcon->font;
    RdtLogicalPoint layer_parent_offset = {};
    bool path_only = evcon->stacking_path && evcon->stacking_path != static_cast<View*>(block);
    if (evcon->stacking_path == static_cast<View*>(block)) {
        layer_parent_offset = layout_positioned_parent_offset(block);
        evcon->stacking_path = nullptr;
    }
    float pointer_x = evcon->event.mouse_position.x, pointer_y = evcon->event.mouse_position.y;
    RdtMatrix matrix, inverse;
    if (view_get_transform_matrix(static_cast<View*>(block), &matrix)) {
        // inverse transforms compose in traversal order: undo the parent, then its child.
        if (!rdt_matrix_inverse(&matrix, &inverse) ||
            !rdt_matrix_project_point(&inverse, pointer_x, pointer_y,
                &evcon->event.mouse_position.x, &evcon->event.mouse_position.y)) {
            log_leave();
            return; // a singular transform has no hittable painted area.
        }
    }
    RdtLogicalPoint fixed = view_geometry_fixed_scroll_adjustment(block);
    evcon->block.x = pa_block.x + block->x + fixed.x - layer_parent_offset.x;
    evcon->block.y = pa_block.y + block->y + fixed.y - layer_parent_offset.y;
    MousePositionEvent* event = &evcon->event.mouse_position;
    // an invisible box cannot claim the hit; traversal still reaches visible descendants.
    bool self_unhittable = view_visibility_is_hidden(block) || event_view_pointer_events_none(
        static_cast<View*>(block));
    bool content_phase = render_paint_phase_has_content(evcon->paint_phase);
    // ordinary block self hits belong below floats; replaced content remains foreground.
    bool phase_self_allowed = evcon->paint_phase == RADIANT_PAINT_ALL ||
        evcon->paint_phase == RADIANT_PAINT_BLOCK_BACKGROUNDS ||
        (content_phase && layout_tag_is_replaced_content((NameId)block->tag()));
    // Content outside the overflow clip is not painted (setup_scroller), so it
    // must not take the hit either: an overflowing child would claim clicks on
    // a later sibling painted over it, e.g. a tab bar under a long <pre>. The
    // clip is in unscrolled block space, so test it before the scroll offset.
    Bound overflow_clip; Corner overflow_radius;
    bool clipped_out = render_clip_overflow_geometry(block, &overflow_clip, &overflow_radius) &&
        !view_is_top_level_viewport(block) &&
        ((layout_block_clips_overflow_axis(block, true) &&
            (event->x - evcon->block.x < overflow_clip.left ||
             event->x - evcon->block.x >= overflow_clip.right)) ||
         (layout_block_clips_overflow_axis(block, false) &&
            (event->y - evcon->block.y < overflow_clip.top ||
             event->y - evcon->block.y >= overflow_clip.bottom)) ||
         (radiant_corner_has_radius(&overflow_radius) &&
          !clip_point_in_rounded_rect(event->x - evcon->block.x, event->y - evcon->block.y,
            {overflow_clip.left, overflow_clip.top, overflow_clip.right - overflow_clip.left,
             overflow_clip.bottom - overflow_clip.top}, &overflow_radius)));
    // target the scrollbars first
    View* view = NULL;
    bool hover = false;
    if (block->scroller && block->scroll_mut()->pane) {
        hover = content_phase && !path_only && !self_unhittable && scrollpane_target(evcon, block);
        if (hover) {
            log_debug("hit on block scroll: %s", block->node_name());
            evcon->target = static_cast<View*>(block);
            evcon->offset_x = event->x - evcon->block.x;
            evcon->offset_y = event->y - evcon->block.y;
            goto RETURN;
        }
        else {
            log_debug("hit not on block scroll");
        }
        // setup scrolling offset
        DocState* state = event_view_owner_state(static_cast<View*>(block));
        if (!state) state = event_context_target_state(evcon);
        float scroll_x = 0.0f, scroll_y = 0.0f;
        scroll_state_get_position_for_view(state, static_cast<View*>(block), block->scroll()->pane,
                                           &scroll_x, &scroll_y, NULL, NULL);
        // hidden overflow scrolls without scrollbars; visible/clip axes do not scroll.
        const ScrollProp* scroll = block->scroll();
        if (!scroll->has_hz_scroll && scroll->overflow_x != CSS_VALUE_HIDDEN) scroll_x = 0.0f;
        if (!scroll->has_vt_scroll && scroll->overflow_y != CSS_VALUE_HIDDEN) scroll_y = 0.0f;
        evcon->block.x -= scroll_x;
        evcon->block.y -= scroll_y;
    }

    // Check if this block is a child-window webview — stop hit-testing here.
    // In child-window mode, the OS delivers events directly to the native web view.
    // Radiant should not process events that land inside the webview area.
    if (!self_unhittable && block->embed && block->embedp()->webview &&
        block->embedp()->webview->mode == WEBVIEW_MODE_WINDOW) {
        float bx = evcon->block.x, by = evcon->block.y;
        MousePositionEvent* mev = &evcon->event.mouse_position;
        if (bx <= mev->x && mev->x < bx + block->width &&
            by <= mev->y && mev->y < by + block->height) {
            log_debug("hit on webview (child-window mode), stopping: %s", block->node_name());
            evcon->target = static_cast<View*>(block);
            evcon->offset_x = mev->x - bx;
            evcon->offset_y = mev->y - by;
            goto RETURN;
        }
    }

    if (block->font) {
        setup_font(evcon->ui_context, &evcon->font, block->font);
    }

    // children, positioned ones included, paint inside the clip; the block's
    // own border box stays hittable below
    if (clipped_out || view_contents_are_skipped(block)) goto RETURN;

    if (path_only) {
        target_stacking_view(evcon, radiant_stack_path_child(block, evcon->stacking_path));
        goto RETURN;
    }

    // Positioned content paints after a custom layout's local signed-z sequence.
    // Hit testing must consume those same layers in exact reverse paint order.
    if (content_phase && block->custom_layout_paint_prop()) {
        target_positioned_children(evcon, block);
        if (evcon->target) goto RETURN;
        target_custom_layout_children(evcon, block);
        if (evcon->target) goto RETURN;
    } else if (content_phase) {
        // Positioned content paints above the block's embedded self-content; walking
        // it first prevents iframe/webview hit targets from stealing covered clicks.
        target_positive_z_descendants(evcon, block->first_child);
        if (evcon->target) goto RETURN;
        target_positioned_children(evcon, block);
        if (evcon->target) goto RETURN;
    }

    // Layer-mode webview: Radiant owns events but forwards them to the offscreen web view.
    // Set target to the webview block and inject the mouse event.
    if (!self_unhittable && block->embed && block->embedp()->webview &&
        block->embedp()->webview->mode == WEBVIEW_MODE_LAYER &&
        block->embedp()->webview->handle) {
        float bx = evcon->block.x, by = evcon->block.y;
        MousePositionEvent* mev = &evcon->event.mouse_position;
        if (bx <= mev->x && mev->x < bx + block->width &&
            by <= mev->y && mev->y < by + block->height) {
            log_debug("hit on webview (layer mode), forwarding event: %s", block->node_name());
            evcon->target = static_cast<View*>(block);
            evcon->offset_x = mev->x - bx;
            evcon->offset_y = mev->y - by;

            // translate to webview-local coordinates and inject
            float local_x = mev->x - bx;
            float local_y = mev->y - by;
            // mouse type: 2=mousemove for hover, 3=click for press (injected on actual click)
            webview_layer_platform_inject_mouse(block->embedp()->webview->handle,
                2, local_x, local_y, 0, 0);
            goto RETURN;
        }
    }

    // Check if this block contains an embedded iframe document
    // If so, target into the iframe's document instead of treating it as a normal block
    if (!self_unhittable && block->embed && block->embedp()->doc) {
        DomDocument* iframe_doc = block->embedp()->doc;
        if (iframe_doc->view_tree && iframe_doc->view_tree->root) {
            log_debug("targeting into iframe embedded document: %s", block->node_name());

            // Save current state
            View* prev_target = evcon->target;
            DomDocument* prev_target_document = evcon->target_document;

            // Target into the embedded document's view tree
            // The coordinate system is already set up correctly (evcon->block.x/y)
            // since we added block->x and block->y above
            evcon->target_document = iframe_doc;
            target_html_doc(evcon, iframe_doc->view_tree);

            // If we found a target inside the iframe, we're done
            if (evcon->target && evcon->target != prev_target) {
                log_debug("found target inside iframe: %s",
                    evcon->target->is_element() ? (lam::view_require_element(evcon->target))->node_name() : "text");
                // Record the iframe block so events can propagate across
                // the iframe boundary back into the parent document
                evcon->iframe_container = static_cast<View*>(block);
                goto RETURN;
            }

            evcon->target_document = prev_target_document;
            log_debug("no target found inside iframe, will target iframe block itself");
        }
    }

    // target static positioned children
    view = block->custom_layout_paint_prop() ? nullptr : block->first_child;
    if (view) {
        target_children(evcon, view);
        bool rich_host_margin_hit_allowed = event_inside_block(evcon, block);
        bool rich_host = radiant_hit_rich_editable_host(static_cast<View*>(block));
        if (!rich_host_margin_hit_allowed && rich_host) {
            // margin caret snapping belongs to this parent's gaps; it must not
            // claim painted siblings or distant content during reverse hit traversal.
            ViewBlock* parent_block = lam::view_as_block(block->parent);
            rich_host_margin_hit_allowed = parent_block &&
                pa_block.x <= event->x && event->x < pa_block.x + parent_block->width &&
                pa_block.y <= event->y && event->y < pa_block.y + parent_block->height;
            for (View* sibling = parent_block ? parent_block->first_child : nullptr;
                 sibling; sibling = sibling->next_sibling) {
                if (sibling == static_cast<View*>(block)) continue;
                if (sibling->view_type != RDT_VIEW_BLOCK &&
                    sibling->view_type != RDT_VIEW_INLINE_BLOCK &&
                    sibling->view_type != RDT_VIEW_LIST_ITEM) {
                    continue;
                }
                ViewBlock* sibling_block = lam::view_require_block(sibling);
                float sibling_x = pa_block.x + sibling_block->x;
                float sibling_y = pa_block.y + sibling_block->y;
                if (sibling_x <= event->x && event->x < sibling_x + sibling_block->width &&
                    sibling_y <= event->y && event->y < sibling_y + sibling_block->height) {
                    rich_host_margin_hit_allowed = false;
                    break;
                }
            }
        }
        if (phase_self_allowed && !self_unhittable && !evcon->target && is_in_rich_editable_subtree(static_cast<View*>(block)) &&
            rich_host_margin_hit_allowed) {
            EditableMarginTextHit margin_hit = { NULL, NULL, 0, 0.0f, 0.0f, -1.0f };
            bool include_vertical_gap = rich_host;
            for (View* child = view; child; child = child->next()) {
                if (!child->view_type) continue;
                find_editable_margin_text_hit(evcon, child, evcon->block.x, evcon->block.y,
                                              &margin_hit, include_vertical_gap);
            }
            if (margin_hit.text && margin_hit.rect) {
                evcon->target = static_cast<View*>(margin_hit.text);
                evcon->target_text_rect = margin_hit.rect;
                evcon->target_text_offset_valid = true;
                evcon->target_text_offset = margin_hit.offset;
                evcon->block.x = margin_hit.block_x;
                evcon->block.y = margin_hit.block_y;
                log_debug("editable margin text hit: text=%p start=%d len=%d offset=%d block=(%.1f,%.1f) score=%.1f",
                          margin_hit.text, margin_hit.rect->start_index,
                          margin_hit.rect->length, margin_hit.offset,
                          margin_hit.block_x, margin_hit.block_y, margin_hit.score);
            }
        }
    }

    if (content_phase) target_negative_z_descendants(evcon, block);

    RETURN:
    // Only restore block position if no target was found
    // When a target is found, keep block at the parent's position for coordinate calculations
    if (!evcon->target) {
        evcon->block = pa_block;
    }
    evcon->font = pa_font;

    // A replaced element (image, etc.) inside a rich editable IS a hit-testable
    // block — clicking it must target the element so the editor can select it.
    // Without this, block hit-testing is skipped for editable content (which is
    // assumed to be all text) and the click snaps to the nearest text via the
    // margin-text-hit above, so the image can never be clicked/selected.
    uintptr_t self_tag = block->tag();
    bool is_replaced_block = layout_tag_is_replaced_content((NameId)self_tag) ||
        self_tag == MARKUP_NAME_HR || self_tag == MARKUP_NAME_OBJECT;
    // cell padding belongs to the cell, even when nearby editable text is a caret candidate.
    bool is_table_cell = block->view_type == RDT_VIEW_TABLE_CELL;
    if (phase_self_allowed && !path_only && !self_unhittable && !evcon->target &&
        (is_replaced_block || is_table_cell ||
         !(is_in_rich_editable_subtree(static_cast<View*>(block)) && !radiant_hit_rich_editable_host(static_cast<View*>(block))))) { // check the block itself
        // use the block's own accumulated position (parent + block offset),
        // not the restored parent position
        float x = pa_block.x + block->x + fixed.x - layer_parent_offset.x;
        float y = pa_block.y + block->y + fixed.y - layer_parent_offset.y;
        BorderProp* border = block->bound ? block->boundary_mut()->border.get() : nullptr;
        Corner radius = {};
        if (border) {
            resolve_border_radius_percentages(&border->radius, block->width, block->height);
            radius = border->radius;
            constrain_corner_radii(&radius, block->width, block->height);
        }
        if (x <= event->x && event->x < x + block->width &&
            y <= event->y && event->y < y + block->height &&
            clip_point_in_rounded_rect(event->x, event->y, {x, y, block->width, block->height}, &radius)) {
            // A fixed editor overlay can cover the viewport visually while
            // pointer-events:none makes the underlying control the hit target.
            log_debug("hit on block: %s", block->node_name());
            evcon->target = static_cast<View*>(block);
            evcon->block.x = x - block->x;
            evcon->block.y = y - block->y;
            evcon->offset_x = event->x - x;
            evcon->offset_y = event->y - y;
        }
        else {
            log_debug("hit not on block: %s, x: %.1f, y: %.1f, ex: %.1f, ey: %.1f, right: %.1f, bottom: %.1f",
                block->node_name(), x, y, event->x, event->y, x + block->width, y + block->height);
        }
    }
    // keep the local point through the block's own test, then restore it for siblings.
    evcon->event.mouse_position.x = pointer_x;
    evcon->event.mouse_position.y = pointer_y;
    log_leave();
}

void target_html_doc(EventContext* evcon, ViewTree* view_tree) {
    if (!evcon || !view_tree) return;
    // embedded documents own their complete stack, even inside an outer foreground visit.
    RadiantPaintPhaseScope document_phase(&evcon->paint_phase, RADIANT_PAINT_ALL);
    float saved_x = evcon->viewport_pointer_x, saved_y = evcon->viewport_pointer_y;
    evcon->viewport_pointer_x = evcon->event.mouse_position.x;
    evcon->viewport_pointer_y = evcon->event.mouse_position.y;
    View* root_view = view_tree->root;
    if (root_view && root_view->view_type == RDT_VIEW_BLOCK) {
        // Pointer hits start in this tree, independently of keyboard focus in an iframe.
        evcon->target_document = lam::view_require_block(root_view)->doc;
        evcon->iframe_container = nullptr;
        log_debug("target root view");
        FontBox pa_font = evcon->font;
        FontProp* default_font = view_tree->html_version == HTML5 ? &evcon->ui_context->default_font : &evcon->ui_context->legacy_default_font;
        log_debug("target_html_doc default font: %s, html version: %d", default_font->family, view_tree->html_version);
        setup_font(evcon->ui_context, &evcon->font, default_font);
        target_block_view(evcon, lam::view_require_block(root_view));
        evcon->font = pa_font;
    }
    else {
        log_error("Invalid root view: %d", root_view ? root_view->view_type : -1);
    }
    evcon->viewport_pointer_x = saved_x; evcon->viewport_pointer_y = saved_y;
}


static void* radiant_element_from_point(DomDocument* doc, DomElement* root, float x, float y) {
    if (!doc || !doc->view_tree || !doc->view_tree->root ||
        !doc->js.host_ui_context) return nullptr;
    UiContext* uicon = (UiContext*)doc->js.host_ui_context;
    RdtEvent hit_event = {};
    hit_event.type = RDT_EVENT_MOUSE_MOVE;
    hit_event.mouse_position.x = x;
    hit_event.mouse_position.y = y;
    // A point query uses native geometry only and does not create script roots.
    EventContext evcon = {};
    evcon.ui_context = uicon;
    evcon.target_document = doc;
    evcon.event = hit_event;
    evcon.viewport_pointer_x = x;
    evcon.viewport_pointer_y = y;
    DomDocument* saved_document = uicon->document;
    uicon->document = lam::up(doc);
    setup_font(uicon, &evcon.font, &uicon->default_font);
    radiant_document_ensure_state(doc, "element_from_point");
    if (root) {
        float frame[6];
        if (dom_svg_foreign_object_client_transform(root, frame)) {
            evcon.viewport_pointer_x = frame[0] * x + frame[1] * y + frame[2];
            evcon.viewport_pointer_y = frame[3] * x + frame[4] * y + frame[5];
        }
        ViewBlock* block = lam::view_as_block(root);
        if (block) target_block_view(&evcon, block);
    } else target_html_doc(&evcon, doc->view_tree);
    uicon->document = lam::up(saved_document);
    DomElement* element = view_geometry_nearest_dom_element(
        static_cast<DomNode*>(evcon.target), 0);
    void* hit = element;
    return hit;
}

void* radiant_document_element_from_point(DomDocument* doc, float x, float y) {
    return radiant_element_from_point(doc, nullptr, x, y);
}

void* radiant_subtree_element_from_point(DomElement* root, float x, float y) {
    return root ? radiant_element_from_point(root->doc, root, x, y) : nullptr;
}
