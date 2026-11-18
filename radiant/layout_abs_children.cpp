#include "layout.hpp"

#include "lambda/input/css/css_style_node.hpp"
#include "tagged.hpp"
#include <cmath>

void layout_abs_block(LayoutContext* lycon, DomNode *elmt, ViewBlock* block,
    BlockContext *pa_block, Linebox *pa_line);

static float layout_abs_child_aspect_ratio(ViewBlock* block) {
    if (!block || !block->specified_style) return 0.0f;

    CssDeclaration* ar_decl = style_tree_get_declaration(
        block->specified_style, CSS_PROPERTY_ASPECT_RATIO);
    if (!ar_decl || !ar_decl->value) return 0.0f;

    return layout_aspect_ratio_value(ar_decl->value);
}

static void layout_apply_abs_child_aspect_ratio(AbsChildLayoutState* state) {
    if (!state || !state->child_block) return;

    ViewBlock* child = state->child_block;
    float aspect_ratio = layout_abs_child_aspect_ratio(child);
    if (aspect_ratio <= 0.0f) return;

    bool has_explicit_height = state->original_given_height > 0.0f;
    bool has_explicit_width = state->original_given_width > 0.0f;
    if (child->width > 0.0f && !has_explicit_height) {
        child->height = child->width / aspect_ratio;
        log_debug("[LAYOUT_ABS] aspect-ratio height: width=%.1f ratio=%.3f height=%.1f",
                  child->width, aspect_ratio, child->height);
    } else if (child->height > 0.0f && !has_explicit_width && child->width <= 0.0f) {
        child->width = child->height * aspect_ratio;
        log_debug("[LAYOUT_ABS] aspect-ratio width: height=%.1f ratio=%.3f width=%.1f",
                  child->height, aspect_ratio, child->width);
    }
}

void layout_absolute_children_in_context(LayoutContext* lycon, ViewBlock* container,
    AbsStaticContext* ctx) {
    if (!lycon || !container || !ctx || lycon->run_mode == radiant::RunMode::MeasureFlow) return;

    // the scope owns the flattened list through recursive child layout.
    int capacity = layout_count_flattened_item_nodes(container, false);
    if (capacity <= 0) return;
    ScratchScope scope(&lycon->scratch);
    DomNode** nodes = scope.array_zero<DomNode*>((size_t)capacity);
    if (!nodes) {
        log_error("[LAYOUT_ABS] unable to collect positioned child boxes");
        return;
    }
    LayoutFlattenedItemPolicy policy = {};
    int count = layout_collect_flattened_item_nodes(lycon, container,
        container->first_child, nodes, capacity, &policy);
    for (int index = 0; index < count; index++) {
        DomNode* child = nodes[index];
        ViewBlock* child_block = lam::view_as_block(child->as_element());
        if (!layout_view_is_abs_or_fixed(child_block)) continue;

        AbsChildLayoutState state = {};
        state.child = lam::up(child);
        state.child_block = lam::up(child_block);
        state.containing_block = ctx->containing_block.view
            ? ctx->containing_block
            : layout_containing_block_for_view(container);
        state.parent_block = lycon->block;
        state.parent_line = lycon->line;
        LayoutContextScope child_scope(lycon);

        if (child_block->blk) {
            lycon->block.given_width = child_block->block()->given_width;
            lycon->block.given_height = child_block->block()->given_height;
            if (ctx->resolve_percent_against_content_box) {
                layout_resolve_percent_size_for_child(lycon, child_block,
                    state.containing_block, true);
            }
        } else {
            lycon->block.given_width = -1.0f;
            lycon->block.given_height = -1.0f;
        }

        state.original_given_width = lycon->block.given_width;
        state.original_given_height = lycon->block.given_height;

        lycon->abspos_static_size_override_x = false;
        lycon->abspos_static_size_override_y = false;
        if (ctx->prepare_child) {
            ctx->prepare_child(lycon, container, ctx, &state);
        }

        // grid placement must constrain the contents, not resize the outer box afterward.
        lycon->abspos_grid_area = state.has_grid_area ? lam::up(&state) : nullptr;
        layout_abs_block(lycon, child, child_block, &state.parent_block, &state.parent_line);

        if (ctx->after_child) {
            ctx->after_child(lycon, container, ctx, &state);
        }

        layout_apply_abs_child_aspect_ratio(&state);
    }
}
