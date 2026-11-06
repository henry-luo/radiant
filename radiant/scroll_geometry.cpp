#include "scroll_geometry.hpp"
#include "view.hpp"

static bool dom_scrollable_overflow_is_clipped(const DomElement* elem,
                                               bool width_axis) {
    if (!elem || !elem->scroller) return false;
    CssEnum overflow = width_axis ? elem->scroll()->overflow_x
                                  : elem->scroll()->overflow_y;
    return overflow == CSS_VALUE_AUTO || overflow == CSS_VALUE_SCROLL ||
           overflow == CSS_VALUE_HIDDEN || overflow == CSS_VALUE_CLIP;
}

static bool dom_scrollable_overflow_is_fixed(const DomElement* elem) {
    return elem && elem->position &&
        elem->positionp()->position == CSS_VALUE_FIXED;
}

// parent_x/parent_y: the content origin the node's x/y are relative to,
// measured from the scroll container's border box.
static float dom_scrollable_descendant_extent(DomNode* node, bool width_axis,
                                              float parent_x, float parent_y) {
    if (!node || !node->is_element()) return 0.0f;
    DomElement* elem = node->as_element();
    if (!elem || dom_scrollable_overflow_is_fixed(elem)) return 0.0f;

    float end = width_axis ? parent_x + elem->x + elem->width
                           : parent_y + elem->y + elem->height;
    if (dom_scrollable_overflow_is_clipped(elem, width_axis)) {
        return max(end, 0.0f);
    }
    // view x/y are relative to the parent block, so nested descendants need
    // their ancestors' offsets; without them a multicol body's overflow
    // columns lost the body's own inset (CSSOM scrollWidth stopped short).
    RdtLogicalPoint child_origin = view_geometry_child_content_origin(node, {parent_x, parent_y});
    float child_x = child_origin.x;
    float child_y = child_origin.y;
    for (DomNode* child = elem->first_child; child; child = child->next_sibling) {
        end = max(end, dom_scrollable_descendant_extent(child, width_axis, child_x, child_y));
    }
    return max(end, 0.0f);
}

extern "C" float dom_scrollable_extent(void* element, bool width_axis) {
    DomElement* elem = static_cast<DomElement*>(element);
    if (!elem) return 0.0f;
    float extent = max(width_axis ? elem->content_width : elem->content_height,
                       width_axis ? elem->width : elem->height);
    // a block's children are placed relative to its border box; an inline
    // container's children share its own containing-block origin.
    float origin_x = elem->is_block() ? 0.0f : -elem->x;
    float origin_y = elem->is_block() ? 0.0f : -elem->y;
    float descendant_extent = 0.0f;
    for (DomNode* child = elem->first_child; child; child = child->next_sibling) {
        descendant_extent = max(descendant_extent,
            dom_scrollable_descendant_extent(child, width_axis, origin_x, origin_y));
    }
    // Scrollable overflow includes the padding after overflowing descendants.
    // Flex boxes with overflow: clip otherwise lose that padding because their
    // cached content size stops at the child edge.
    float padding_end = elem->bound
        ? (width_axis ? elem->boundary()->padding.right
                      : elem->boundary()->padding.bottom) : 0.0f;
    extent = max(extent, descendant_extent + padding_end);
    return extent;
}

