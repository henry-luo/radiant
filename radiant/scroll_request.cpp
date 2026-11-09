#include "scroll_request.hpp"
#include "event.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"

void radiant_queue_scroll_into_view(DomDocument* doc, DomElement* elem, bool center,
                                   bool if_needed, DomScrollAlign block_align, DomScrollAlign inline_align,
                                   DomScrollBehavior behavior) {
    if (!doc || !elem) return;
    if (doc->pending_scroll_into_view_target.address) {
        dom_node_unpin(doc, doc->pending_scroll_into_view_target, DOM_NODE_PIN_RECONCILE);
    }
    doc->pending_scroll_into_view_target = {};
    doc->pending_scroll_into_view_center = false;
    doc->pending_scroll_into_view_if_needed = false;
    doc->pending_scroll_into_view_block = DOM_SCROLL_ALIGN_START;
    doc->pending_scroll_into_view_inline = DOM_SCROLL_ALIGN_NEAREST;
    doc->pending_scroll_into_view_behavior = DOM_SCROLL_BEHAVIOR_AUTO;
    DomNodeRef ref = dom_node_ref((DomNode*)elem);
    if (!dom_node_ref_validate(doc, ref) || !dom_node_pin(doc, ref, DOM_NODE_PIN_RECONCILE)) return;
    doc->pending_scroll_into_view_target = ref;
    doc->pending_scroll_into_view_center = center;
    doc->pending_scroll_into_view_if_needed = if_needed;
    doc->pending_scroll_into_view_block = block_align;
    doc->pending_scroll_into_view_inline = inline_align;
    doc->pending_scroll_into_view_behavior = behavior;
    // scrolling changes presentation; the geometry checkpoint validates flow separately.
    doc_state_request_repaint(doc->state);
}
