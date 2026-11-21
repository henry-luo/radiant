#include "layout_anchor.hpp"
#include "event.hpp"
#include "tagged.hpp"
#include "lib/arraylist.hpp"
#include "lib/hashmap_typed.hpp"
#include "lib/log.h"
#include "lambda/input/css/css_style_node.hpp"
#include <math.h>

struct AnchorGeometry {
    DomElement* element;
    DomElement* containing;
    size_t layer;
    Rect bounds;
    RdtLogicalPoint origin;
    bool positioned;
};

struct AnchorIndex {
    DomElement* element;
    size_t order;
};
using AnchorIndexMap = TypedHashMap<AnchorIndex,
    HashMapPointerMemberKeyOps<AnchorIndex, &AnchorIndex::element>>;

struct LayoutAnchorSnapshot {
    lam::ArrayList<AnchorGeometry> boxes{MEM_CAT_LAYOUT, 0};
    lam::ArrayList<size_t> named{MEM_CAT_LAYOUT, 0};
    AnchorIndexMap index{};
    RdtLogicalPoint scroll{};

    ~LayoutAnchorSnapshot() { index.destroy(); }
    const AnchorIndex* lookup(DomElement* element) const { return index.get({element, 0}); }
    const AnchorGeometry* find(DomElement* element) const {
        const AnchorIndex* entry = lookup(element);
        return entry ? &boxes[entry->order] : nullptr;
    }
};

static const CssValue* anchor_style(LayoutContext* lycon, DomElement* element, CssPropertyCode property) {
    if (!element || !element->specified_style) return nullptr;
    CssDeclaration* decl = style_tree_get_declaration(element->specified_style, property);
    const CssValue* value = decl ? decl->value : nullptr;
    if (value && css_value_contains_pending_substitution(value)) {
        LayoutViewScope restore(lycon);
        lycon->view = lam::up(static_cast<View*>(element));
        lycon->elmt = lam::up(static_cast<DomNode*>(element));
        return resolve_var_function(lycon, value);
    }
    return value;
}

static DomElement* anchor_containing_element(ViewBlock* block) {
    ViewBlock* containing = find_containing_block(block, block->positionp()->position);
    DomElement* element = containing ? containing->as_element() : nullptr;
    return element && !element->is_synthetic() ? element : nullptr;
}

static bool anchor_capture_tree(LayoutContext* lycon, LayoutAnchorSnapshot& snapshot,
                                View* view, size_t layer, unsigned depth) {
    DomElement* element = view ? view->as_element() : nullptr;
    if (!element || depth >= MAX_LAYOUT_DEPTH || layout_display_is_none(element->display)) return true;
    if (element->is_in_top_layer()) {
        for (size_t i = dom_top_layer_count(lycon->doc); i > 0; --i)
            if (dom_top_layer_at(lycon->doc, i - 1) == element) { layer = i; break; }
    }
    if (view->view_type != RDT_VIEW_NONE && !element->is_synthetic()) {
        AnchorGeometry geometry{};
        geometry.element = element;
        geometry.layer = layer;
        geometry.positioned = layout_view_is_out_of_flow_positioned(view);
        if (geometry.positioned) geometry.containing = anchor_containing_element(lam::view_require_block(view));
        else {
            for (ViewElement* parent = layout_render_parent(view); parent;
                 parent = layout_render_parent(parent)) {
                if (parent->is_block() && !parent->is_synthetic()) {
                    geometry.containing = parent->as_element();
                    break;
                }
            }
        }
        view_get_visual_bounds(view, &geometry.bounds.x, &geometry.bounds.y,
                               &geometry.bounds.width, &geometry.bounds.height);
        view_get_layout_position(view, nullptr, &geometry.origin.x, &geometry.origin.y);
        size_t order = snapshot.boxes.size();
        snapshot.index.set({element, order});
        if (snapshot.index.oom() || !snapshot.boxes.push_back(geometry)) return false;
        const CssValue* names = anchor_style(lycon, element, CSS_PROPERTY_ANCHOR_NAME);
        if (names && css_anchor_name(css_value_comma_at(names, 0)) &&
            !snapshot.named.push_back(order)) return false;
    }
    if (!view_contents_are_skipped(view)) {
        for (View* child = layout_paint_first_child(element); child; child = layout_paint_next(child))
            if (!anchor_capture_tree(lycon, snapshot, child, layer, depth + 1)) return false;
    }
    return true;
}

static bool anchor_geometry_equal(const LayoutAnchorSnapshot& a, const LayoutAnchorSnapshot& b) {
    if (a.boxes.size() != b.boxes.size() || a.named.size() != b.named.size() ||
        a.scroll.x != b.scroll.x || a.scroll.y != b.scroll.y) return false;
    for (size_t i = 0; i < a.boxes.size(); i++) {
        const AnchorGeometry& x = a.boxes[i];
        const AnchorGeometry& y = b.boxes[i];
        if (x.element != y.element || x.containing != y.containing || x.layer != y.layer ||
            x.positioned != y.positioned || x.origin.x != y.origin.x || x.origin.y != y.origin.y ||
            x.bounds.x != y.bounds.x || x.bounds.y != y.bounds.y ||
            x.bounds.width != y.bounds.width || x.bounds.height != y.bounds.height) return false;
    }
    for (size_t i = 0; i < a.named.size(); i++)
        if (a.named[i] != b.named[i]) return false;
    return true;
}

LayoutAnchorPass::~LayoutAnchorPass() {
    LayoutAnchorSnapshot* snapshots[] = {previous, current};
    for (LayoutAnchorSnapshot* snapshot : snapshots) {
        if (!snapshot) continue;
        snapshot->~LayoutAnchorSnapshot();
        mem_free(snapshot);
    }
}

void LayoutAnchorPass::begin(LayoutContext* lycon) const {
    lycon->anchor_snapshot = lam::up(previous);
}

bool LayoutAnchorPass::settle(LayoutContext* lycon, View* root) {
    if (!lycon->anchor_dependencies || lycon->paged) return false;
    if (!current) {
        void* storage = mem_alloc(sizeof(LayoutAnchorSnapshot), MEM_CAT_LAYOUT);
        if (!storage) return false;
        current = new (storage) LayoutAnchorSnapshot();
    }
    if (!current->index.initialized() && !current->index.init(128)) return false;
    current->boxes.clear();
    current->named.clear();
    hashmap_clear(current->index.raw(), true);
    ViewBlock* viewport = lam::view_as_block(root);
    current->scroll = {};
    if (viewport) scroll_state_resolve_view_geometry(viewport, &current->scroll.x, &current->scroll.y, nullptr);
    if (!anchor_capture_tree(lycon, *current, root, 0, 0)) return false;
    if (previous && anchor_geometry_equal(*previous, *current)) return false;
    // acceptable anchors give the dependency graph a strict layout order.
    if (++rounds > lycon->anchor_dependencies + 1) {
        log_error("anchor-positioning: geometry did not settle after %zu passes", rounds);
        return false;
    }
    LayoutAnchorSnapshot* old = previous;
    previous = current;
    current = old;
    return true;
}

static bool anchor_acceptable(const LayoutAnchorSnapshot& snapshot, size_t candidate, size_t target) {
    const AnchorGeometry& query = snapshot.boxes[target];
    // a containing-block chain must reach the target's original containing block.
    for (size_t depth = 0; depth <= snapshot.boxes.size(); depth++) {
        const AnchorGeometry& box = snapshot.boxes[candidate];
        if (candidate == target || box.layer > query.layer) return false;
        if (box.element == query.containing) return true;
        if (box.containing == query.containing) {
            return box.layer < query.layer || !box.positioned || candidate < target;
        }
        const AnchorIndex* containing = snapshot.lookup(box.containing);
        if (!containing) return false;
        candidate = containing->order;
    }
    return false;
}

static DomElement* anchor_scope(LayoutContext* lycon, DomElement* element, const char* name) {
    DomElement* root = dom_shadow_root(element);
    for (; element && dom_shadow_root(element) == root; element = dom_flat_tree_parent(element)) {
        const CssValue* scope = anchor_style(lycon, element, CSS_PROPERTY_ANCHOR_SCOPE);
        if (css_value_keyword_equals(scope, CSS_VALUE_ALL) || css_anchor_name_contains(scope, name))
            return element;
    }
    return nullptr;
}

static const AnchorGeometry* anchor_lookup(LayoutContext* lycon, DomElement* target, size_t target_index,
                                          const char* name, bool implicit) {
    const LayoutAnchorSnapshot& snapshot = *lycon->anchor_snapshot;
    if (implicit) {
        const PopoverStyle* state = target->popover_style();
        DomNode* source = state ? dom_node_ref_validate(target->doc, state->implicit_anchor) : nullptr;
        const AnchorIndex* index = source ? snapshot.lookup(source->as_element()) : nullptr;
        return index && anchor_acceptable(snapshot, index->order, target_index)
            ? &snapshot.boxes[index->order] : nullptr;
    }
    DomElement* root = dom_shadow_root(target);
    DomElement* scope = anchor_scope(lycon, target, name);
    auto matches = [&](size_t entry) {
        DomElement* element = snapshot.boxes[entry].element;
        return dom_shadow_root(element) == root && anchor_acceptable(snapshot, entry, target_index) &&
            css_anchor_name_contains(anchor_style(lycon, element, CSS_PROPERTY_ANCHOR_NAME), name) &&
            anchor_scope(lycon, element, name) == scope;
    };
    for (DomElement* parent = dom_flat_tree_parent(target); parent; parent = dom_flat_tree_parent(parent)) {
        const AnchorIndex* entry = snapshot.lookup(parent);
        if (entry && matches(entry->order)) return &snapshot.boxes[entry->order];
    }
    for (size_t i = snapshot.named.size(); i > 0; --i) {
        size_t entry = snapshot.named[i - 1];
        if (matches(entry)) return &snapshot.boxes[entry];
    }
    return nullptr;
}

static const AnchorGeometry* default_anchor(LayoutContext* lycon, DomElement* target,
                                           size_t target_index, unsigned depth = 0) {
    if (!target || depth >= MAX_LAYOUT_DEPTH) return nullptr;
    const CssValue* value = anchor_style(lycon, target, CSS_PROPERTY_POSITION_ANCHOR);
    if (const char* name = css_anchor_name(value)) return anchor_lookup(lycon, target, target_index, name, false);
    if (css_value_identifier_is(value, "match-parent"))
        return default_anchor(lycon, dom_flat_tree_parent(target), target_index, depth + 1);
    if (css_value_keyword_equals(value, CSS_VALUE_NONE)) return nullptr;
    // the HTML invoker supplies an implicit anchor when no explicit name is used.
    return anchor_lookup(lycon, target, target_index, nullptr, true);
}

static LayoutAxis area_axis(const CssAnchorAreaPart& part, ViewBlock* subject) {
    if (part.axis == CSS_ANCHOR_AXIS_X) return LAYOUT_AXIS_X;
    if (part.axis == CSS_ANCHOR_AXIS_Y) return LAYOUT_AXIS_Y;
    bool inline_x = layout_axis_is_inline_for_block(subject, LAYOUT_AXIS_X);
    return (part.axis == CSS_ANCHOR_AXIS_INLINE) == inline_x ? LAYOUT_AXIS_X : LAYOUT_AXIS_Y;
}

static CssAnchorAreaTrack area_physical_track(CssAnchorAreaPart part, ViewBlock* subject, LayoutAxis axis) {
    if (part.physical || layout_logical_start_is_physical_start(subject, axis)) return part.track;
    switch (part.track) {
        case CSS_ANCHOR_TRACK_START: return CSS_ANCHOR_TRACK_END;
        case CSS_ANCHOR_TRACK_END: return CSS_ANCHOR_TRACK_START;
        case CSS_ANCHOR_SPAN_START: return CSS_ANCHOR_SPAN_END;
        case CSS_ANCHOR_SPAN_END: return CSS_ANCHOR_SPAN_START;
        default: return part.track;
    }
}

static void anchor_apply_area(LayoutAnchorFrame& frame, CssAnchorArea area, const Rect& bounds) {
    unsigned explicit_part = area.part[0].axis == CSS_ANCHOR_AXIS_UNSPECIFIED &&
        area.part[1].axis != CSS_ANCHOR_AXIS_UNSPECIFIED ? 1u : 0u;
    CssAnchorAreaPart part = area.part[explicit_part];
    ViewBlock* subject = part.self ? frame.block : frame.containing;
    LayoutAxis axis = area_axis(part, subject);
    LayoutAxis other = axis == LAYOUT_AXIS_X ? LAYOUT_AXIS_Y : LAYOUT_AXIS_X;
    frame.tracks[axis] = area_physical_track(part, subject, axis);
    part = area.part[1 - explicit_part];
    frame.tracks[other] = area_physical_track(part, part.self ? frame.block : frame.containing, other);
    for (LayoutAxis mapped : layout_axes()) {
        bool horizontal = mapped == LAYOUT_AXIS_X;
        float start = horizontal ? bounds.x - frame.origin.x : bounds.y - frame.origin.y;
        float end = start + (horizontal ? bounds.width : bounds.height);
        float* offset = horizontal ? &frame.box.padding_x : &frame.box.padding_y;
        float* size = horizontal ? &frame.box.padding_width : &frame.box.padding_height;
        float lines[] = {fminf(0.0f, start), start, end, fmaxf(*size, end)};
        static const uint8_t ranges[][2] = {{0,3}, {0,1}, {1,2}, {2,3}, {0,2}, {1,3}};
        const auto& range = ranges[frame.tracks[mapped]];
        *offset += lines[range[0]];
        *size = lines[range[1]] - lines[range[0]];
        if (horizontal) frame.origin.x += lines[range[0]];
        else frame.origin.y += lines[range[0]];
    }
    frame.box.content_width = frame.box.padding_width;
    frame.box.content_height = frame.box.padding_height;
    frame.area = true;
}

static bool anchor_check_declaration(AvlNode* node, void* context) {
    LayoutAnchorFrame* frame = (LayoutAnchorFrame*)context;
    CssPropertyCode property = static_cast<CssPropertyCode>(node->property_id);
    bool geometry = (property >= CSS_PROPERTY_TOP && property <= CSS_PROPERTY_LEFT) ||
        (property >= CSS_PROPERTY_WIDTH && property <= CSS_PROPERTY_MAX_HEIGHT) ||
        (property >= CSS_PROPERTY_MARGIN && property <= CSS_PROPERTY_MARGIN_INLINE_END) ||
        (property >= CSS_PROPERTY_BLOCK_SIZE && property <= CSS_PROPERTY_INSET_INLINE_END);
    if (!geometry) return true;
    frame->active = css_value_contains_anchor_query(anchor_style(frame->lycon, frame->block, property));
    return !frame->active;
}

static RdtLogicalPoint anchor_scroll_origin(DomElement* anchor, ViewBlock* positioned,
                                           ViewBlock* containing) {
    RdtLogicalPoint scroll{};
    if (!anchor || view_is_viewport_fixed(anchor)) return scroll;
    View* root = anchor->doc && anchor->doc->view_tree ? anchor->doc->view_tree->root.get() : nullptr;
    bool fixed_target = positioned->positionp()->position == CSS_VALUE_FIXED && containing == root;
    for (ViewElement* node = layout_render_parent(anchor); node; node = layout_render_parent(node)) {
        bool target_containing_block = node == containing;
        if (target_containing_block && !fixed_target) break;
        if (ViewBlock* scroller = lam::view_as_block(node)) {
            float x = 0.0f, y = 0.0f;
            scroll_state_resolve_view_geometry(scroller, &x, &y, nullptr);
            scroll.x += x;
            scroll.y += y;
        }
        if (target_containing_block || view_is_viewport_fixed(node)) break;
    }
    return scroll;
}

RdtLogicalPoint layout_anchor_scroll_adjustment(ViewBlock* box) {
    if (!box || !box->doc || !box->position || !box->positionp()->anchor_scroll_axes) return {};
    const PositionProp* position = box->positionp();
    DomNode* source = dom_node_ref_validate(box->doc, position->anchor_scroll_target);
    DomElement* anchor = source ? source->as_element() : nullptr;
    if (!anchor) return {};
    ViewBlock* containing = find_containing_block(box, position->position);
    RdtLogicalPoint current = anchor_scroll_origin(anchor, box, containing);
    return {
        (position->anchor_scroll_axes & (1u << LAYOUT_AXIS_X)) ? position->anchor_scroll_origin.x - current.x : 0.0f,
        (position->anchor_scroll_axes & (1u << LAYOUT_AXIS_Y)) ? position->anchor_scroll_origin.y - current.y : 0.0f,
    };
}

LayoutAnchorFrame::LayoutAnchorFrame(LayoutContext* context, ViewBlock* target, ViewBlock* cb,
                                     LayoutContainingBlock original)
    : lycon(context), previous(context->anchor_frame), block(target), containing(cb), box(original) {
    lycon->anchor_frame = lam::up(this);
    if (layout_is_backdrop(block->as_element()) || lycon->paged) return;
    DomElement* element = block->as_element();
    CssAnchorArea area_value{};
    bool area_specified = css_anchor_position_area(anchor_style(lycon, element, CSS_PROPERTY_POSITION_AREA), &area_value);
    bool align = css_value_has_identifier(anchor_style(lycon, element, CSS_PROPERTY_ALIGN_SELF), "anchor-center") ||
        css_value_has_identifier(anchor_style(lycon, element, CSS_PROPERTY_JUSTIFY_SELF), "anchor-center") ||
        css_value_has_identifier(anchor_style(lycon, element, CSS_PROPERTY_PLACE_SELF), "anchor-center");
    active = area_specified || align;
    if (!active && element->specified_style && element->specified_style->tree)
        avl_tree_foreach_inorder(element->specified_style->tree, anchor_check_declaration, this);
    if (!active) return;
    ++lycon->anchor_dependencies;
    if (!lycon->anchor_snapshot) return;
    const AnchorIndex* target_index = lycon->anchor_snapshot->lookup(element);
    if (!target_index) return;
    origin = {box.padding_x, box.padding_y};
    if (const AnchorGeometry* parent = lycon->anchor_snapshot->find(box.view ? box.view->as_element() : nullptr)) {
        origin.x += parent->origin.x;
        origin.y += parent->origin.y;
        if (box.view != lycon->doc->view_tree->root && box.view->scroller && box.view->scroll()->pane) {
            float x = 0.0f, y = 0.0f;
            scroll_state_resolve_view_geometry(box.view, &x, &y, nullptr);
            origin.x -= x;
            origin.y -= y;
        }
    } else if (block->positionp()->position != CSS_VALUE_FIXED && !box.view) {
        origin.x -= lycon->anchor_snapshot->scroll.x;
        origin.y -= lycon->anchor_snapshot->scroll.y;
    }
    if (const AnchorGeometry* anchor = default_anchor(lycon, element, target_index->order)) {
        default_element = anchor->element;
        center = {anchor->bounds.x + anchor->bounds.width / 2.0f,
                  anchor->bounds.y + anchor->bounds.height / 2.0f};
        has_default = true;
        if (area_specified) anchor_apply_area(*this, area_value, anchor->bounds);
    }
}

LayoutAnchorFrame::~LayoutAnchorFrame() {
    if (block->position && (active || block->position->anchor_scroll_axes) &&
        !layout_context_is_measuring(lycon)) {
        // a style change can retire the last query while retaining the positioned view.
        block->position->anchor_scroll_axes = scroll_axes;
        block->position->anchor_scroll_target = dom_node_ref(default_element);
        block->position->anchor_scroll_origin = anchor_scroll_origin(default_element, block, containing);
    }
    lycon->anchor_frame = lam::up(previous);
}

static LayoutAxis anchor_property_axis(CssPropertyCode property, ViewBlock* box) {
    switch (property) {
        case CSS_PROPERTY_TOP: case CSS_PROPERTY_BOTTOM:
        case CSS_PROPERTY_HEIGHT: case CSS_PROPERTY_MIN_HEIGHT: case CSS_PROPERTY_MAX_HEIGHT:
        case CSS_PROPERTY_MARGIN_TOP: case CSS_PROPERTY_MARGIN_BOTTOM: return LAYOUT_AXIS_Y;
        case CSS_PROPERTY_BLOCK_SIZE: case CSS_PROPERTY_MIN_BLOCK_SIZE: case CSS_PROPERTY_MAX_BLOCK_SIZE:
            return layout_axis_is_inline_for_block(box, LAYOUT_AXIS_X) ? LAYOUT_AXIS_Y : LAYOUT_AXIS_X;
        case CSS_PROPERTY_INLINE_SIZE: case CSS_PROPERTY_MIN_INLINE_SIZE: case CSS_PROPERTY_MAX_INLINE_SIZE:
            return layout_axis_is_inline_for_block(box, LAYOUT_AXIS_X) ? LAYOUT_AXIS_X : LAYOUT_AXIS_Y;
        default: return LAYOUT_AXIS_X;
    }
}

bool layout_resolve_anchor_query(LayoutContext* lycon, uintptr_t property,
                                 const CssAnchorQuery& query, double* result) {
    LayoutAnchorFrame* frame = lycon ? lycon->anchor_frame.get() : nullptr;
    if (!frame || !frame->active || lycon->view != frame->block) return false;
    if (!lycon->anchor_snapshot) return false;
    DomElement* element = frame->block->as_element();
    const AnchorIndex* entry = lycon->anchor_snapshot->lookup(element);
    if (!entry) return false;
    const AnchorGeometry* anchor = query.name
        ? anchor_lookup(lycon, element, entry->order, query.name, false)
        : default_anchor(lycon, element, entry->order);
    if (!anchor) return false;
    intptr_t code = (intptr_t)property;
    CssPropertyCode physical = static_cast<CssPropertyCode>(code < 0 ? -code : code);
    LayoutAxis axis = anchor_property_axis(physical, frame->block);
    bool horizontal = axis == LAYOUT_AXIS_X;
    if (query.size) {
        switch (query.side) {
            case CSS_ANCHOR_WIDTH: horizontal = true; break;
            case CSS_ANCHOR_HEIGHT: horizontal = false; break;
            case CSS_ANCHOR_INLINE: case CSS_ANCHOR_BLOCK:
            case CSS_ANCHOR_SELF_INLINE: case CSS_ANCHOR_SELF_BLOCK: {
                bool self = query.side == CSS_ANCHOR_SELF_INLINE || query.side == CSS_ANCHOR_SELF_BLOCK;
                bool is_inline = query.side == CSS_ANCHOR_INLINE || query.side == CSS_ANCHOR_SELF_INLINE;
                horizontal = layout_axis_is_inline_for_block(self ? frame->block : frame->containing,
                    LAYOUT_AXIS_X) == is_inline;
                break;
            }
            default: break;
        }
        *result = horizontal ? anchor->bounds.width : anchor->bounds.height;
        return true;
    }
    bool end_inset = physical == CSS_PROPERTY_RIGHT || physical == CSS_PROPERTY_BOTTOM;
    float fraction = 0.0f;
    switch (query.side) {
        case CSS_ANCHOR_TOP: case CSS_ANCHOR_BOTTOM:
            if (horizontal) return false;
            fraction = query.side == CSS_ANCHOR_BOTTOM ? 1.0f : 0.0f;
            break;
        case CSS_ANCHOR_LEFT: case CSS_ANCHOR_RIGHT:
            if (!horizontal) return false;
            fraction = query.side == CSS_ANCHOR_RIGHT ? 1.0f : 0.0f;
            break;
        case CSS_ANCHOR_INSIDE: case CSS_ANCHOR_OUTSIDE:
            fraction = end_inset == (query.side == CSS_ANCHOR_INSIDE) ? 1.0f : 0.0f;
            break;
        default: {
            bool self = query.side == CSS_ANCHOR_SELF_START || query.side == CSS_ANCHOR_SELF_END;
            fraction = query.side == CSS_ANCHOR_PERCENT ? query.percentage / 100.0f :
                query.side == CSS_ANCHOR_END || query.side == CSS_ANCHOR_SELF_END ? 1.0f : 0.0f;
            if (!layout_logical_start_is_physical_start(self ? frame->block : frame->containing, axis))
                fraction = 1.0f - fraction;
            break;
        }
    }
    float coordinate = horizontal
        ? anchor->bounds.x - frame->origin.x + fraction * anchor->bounds.width
        : anchor->bounds.y - frame->origin.y + fraction * anchor->bounds.height;
    float extent = horizontal ? frame->box.padding_width : frame->box.padding_height;
    *result = end_inset ? extent - coordinate : coordinate;
    if (anchor->element == frame->default_element) frame->scroll_axes |= 1u << axis;
    return isfinite(*result);
}

void layout_apply_anchor_alignment(LayoutAnchorFrame& frame) {
    if (!frame.has_default || !frame.block->blk) return;
    ViewBlock* block = frame.block;
    for (LayoutAxis axis : layout_axes()) {
        CssSelfAlignment& alignment = layout_axis_is_inline_for_block(frame.containing, axis)
            ? block->block_mut()->justify_self : block->block_mut()->align_self;
        LayoutAxisRefs refs(block, axis);
        if (frame.area && (alignment.value == CSS_VALUE_AUTO || alignment.value == CSS_VALUE_NORMAL ||
                           alignment.value == CSS_VALUE__UNDEF)) {
            CssAnchorAreaTrack track = frame.tracks[axis];
            bool single_inset = refs.has_start() != refs.has_end();
            bool toward_end = single_inset ? refs.has_end() :
                track == CSS_ANCHOR_TRACK_START || track == CSS_ANCHOR_SPAN_START;
            bool logical_end = toward_end == layout_logical_start_is_physical_start(frame.containing, axis);
            alignment.value = !single_inset && track == CSS_ANCHOR_SPAN_ALL ? CSS_VALUE_ANCHOR_CENTER :
                !single_inset && track == CSS_ANCHOR_TRACK_CENTER ? CSS_VALUE_CENTER :
                logical_end ? CSS_VALUE_END : CSS_VALUE_START;
            if (single_inset) alignment.overflow_explicit = true;
        }
        if (!frame.area && alignment.value != CSS_VALUE_ANCHOR_CENTER) continue;
        frame.scroll_axes |= 1u << axis;
        if (!refs.has_start()) { *refs.insets.start.has = true; *refs.insets.start.value = 0.0f; }
        if (!refs.has_end()) { *refs.insets.end.has = true; *refs.insets.end.value = 0.0f; }
        if (refs.margin_start_is_auto()) { *refs.margins.start_type = CSS_VALUE__UNDEF; *refs.margins.start = 0.0f; }
        if (refs.margin_end_is_auto()) { *refs.margins.end_type = CSS_VALUE__UNDEF; *refs.margins.end = 0.0f; }
    }
}

bool layout_anchor_center_position(LayoutContext* lycon, ViewBlock* box,
                                   LayoutAxis axis, float min, float max, float margin_box_size,
                                   float* result) {
    LayoutAnchorFrame* frame = lycon ? lycon->anchor_frame.get() : nullptr;
    if (!frame || frame->block != box || !frame->has_default || !box->blk) return false;
    const CssSelfAlignment& alignment = layout_axis_is_inline_for_block(frame->containing, axis)
        ? box->block()->justify_self : box->block()->align_self;
    if (alignment.value != CSS_VALUE_ANCHOR_CENTER) return false;
    float center = axis == LAYOUT_AXIS_X ? frame->center.x - frame->origin.x : frame->center.y - frame->origin.y;
    float start = center - margin_box_size / 2.0f;
    *result = max >= min ? fmaxf(min, fminf(max, start)) : start;
    return true;
}
