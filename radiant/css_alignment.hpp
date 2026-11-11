#pragma once

#include "layout.hpp"

void css_store_content_alignment(LayoutContext* context, ViewBlock* block,
    bool justify, CssSelfAlignment alignment);
void css_store_self_alignment(LayoutContext* context, ViewSpan* span,
    bool justify, CssSelfAlignment alignment);
void css_store_grid_justify_items(LayoutContext* context, ViewBlock* block,
    CssSelfAlignment alignment);
void resolve_flex_grid_container_alignment(LayoutContext* context,
    ViewBlock* block, CssPropertyCode property, const CssValue* value);
void resolve_grid_alignment_property(LayoutContext* context, ViewBlock* block,
    ViewSpan* span, CssPropertyCode property, const CssValue* value);
