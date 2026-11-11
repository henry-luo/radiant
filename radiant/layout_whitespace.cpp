#include "layout.hpp"

CssEnum get_white_space_value(DomNode* node, LayoutContext* context) {
    DomDocument* document = context ? context->doc.get() : nullptr;
    ViewTree* tree = document ? document->view_tree.get() : nullptr;
    DomElement* owner = node && node->parent ? node->parent->as_element() : nullptr;
    bool cacheable = owner && owner->doc == document && tree &&
        !owner->scratch_lived() && !context->css_query_probe &&
        (!context->selected_view_tree || context->selected_view_tree == tree);
    if (!cacheable) {
        if (context) context->white_space_cache.owner = nullptr;
        return get_white_space_value(node);
    }

    auto& cache = context->white_space_cache;
    if (cache.owner != owner || cache.block != owner->blk.get() ||
        cache.style_epoch != document->style_query_epoch ||
        cache.mutation_epoch != document->mutation_epoch ||
        cache.layout_generation != tree->layout_generation ||
        cache.measurement_generation != tree->measurement_cache_generation ||
        cache.view_type != owner->view_type || cache.styles_resolved != owner->styles_resolved()) {
        // sibling text and repeated lookahead share one resolved parent value within this pass.
        cache.value = get_white_space_value(node);
        cache.owner = lam::up(owner);
        cache.block = lam::up(owner->blk.get());
        cache.style_epoch = document->style_query_epoch;
        cache.mutation_epoch = document->mutation_epoch;
        cache.layout_generation = tree->layout_generation;
        cache.measurement_generation = tree->measurement_cache_generation;
        cache.view_type = owner->view_type;
        cache.styles_resolved = owner->styles_resolved();
    }
    return cache.value;
}
