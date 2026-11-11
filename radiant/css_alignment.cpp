#include "css_alignment.hpp"
#include "lib/str.h"
#include <string.h>

void css_store_self_alignment(LayoutContext* lycon, ViewSpan* span,
                                     bool justify, CssSelfAlignment value) {
    if (!span || value.value == CSS_VALUE__UNDEF) return;
    BlockProp* block = span->ensure_block(lycon);
    if (!block) return;
    if (justify) block->justify_self = value;
    else block->align_self = value;
}

void css_store_content_alignment(LayoutContext* lycon, ViewBlock* block,
                                        bool justify, CssSelfAlignment alignment) {
    if (!block) return;
    if (!justify) {
        BlockProp* block_prop = block->ensure_block(lycon);
        block_prop->align_content = alignment.value;
        block_prop->align_content_detail = alignment;
        if (block->display.inner == CSS_VALUE_FLEX) {
            alloc_flex_prop(lycon, block);
            block->embedp()->flex->align_content = alignment.value;
        }
        if (block->display.inner == CSS_VALUE_GRID) {
            alloc_grid_prop(lycon, block);
            block->embedp()->grid->align_content = alignment.value;
        }
        return;
    }
    alloc_flex_prop(lycon, block);
    alloc_grid_prop(lycon, block);
    block->embedp()->flex->justify = alignment.value;
    block->embedp()->grid->justify_content = alignment.value;
}

void resolve_flex_grid_container_alignment(LayoutContext* lycon,
                                                   ViewBlock* block,
                                                   CssPropertyCode property,
                                                   const CssValue* value) {
    if (!block) return;
    if (property == CSS_PROPERTY_ALIGN_CONTENT ||
        property == CSS_PROPERTY_JUSTIFY_CONTENT) {
        bool justify = property == CSS_PROPERTY_JUSTIFY_CONTENT;
        CssSelfAlignment alignment = css_parse_content_alignment_value(value, justify);
        if (alignment.value != CSS_VALUE__UNDEF) {
            css_store_content_alignment(lycon, block, justify, alignment);
        }
        return;
    }
    alloc_flex_prop(lycon, block);
    alloc_grid_prop(lycon, block);
    CssSelfAlignment alignment = css_parse_self_alignment_value(value, false);
    if (alignment.value != CSS_VALUE__UNDEF) {
        block->embedp()->flex->align_items = alignment.value;
        block->embedp()->flex->align_items_detail = alignment;
        block->embedp()->grid->align_items = alignment.value;
        block->embedp()->grid->align_items_detail = alignment;
    }
}

static CssSelfAlignment css_parse_legacy_justify_items(const CssValue* value) {
    CssSelfAlignment result = {};
    if (!value || value->type != CSS_VALUE_TYPE_LIST || value->data.list.count != 2) {
        return result;
    }

    bool has_legacy = false;
    CssEnum position = CSS_VALUE__UNDEF;
    for (int i = 0; i < value->data.list.count; i++) {
        const CssValue* part = value->data.list.values[i];
        if (part && part->type == CSS_VALUE_TYPE_CUSTOM &&
            part->data.custom_property.name &&
            str_ieq_const(part->data.custom_property.name,
                          strlen(part->data.custom_property.name), "legacy")) {
            if (has_legacy) return result;
            has_legacy = true;
            continue;
        }
        if (part && part->type == CSS_VALUE_TYPE_KEYWORD &&
            (part->data.keyword == CSS_VALUE_LEFT ||
             part->data.keyword == CSS_VALUE_RIGHT ||
             part->data.keyword == CSS_VALUE_CENTER)) {
            if (position != CSS_VALUE__UNDEF) return result;
            position = part->data.keyword;
            continue;
        }
        return result;
    }
    if (!has_legacy) return result;
    result.value = position;
    result.legacy = true;
    return result;
}

static CssSelfAlignment css_parse_justify_items(const CssValue* value) {
    CssSelfAlignment result = css_parse_self_alignment_value(value, true);
    if (result.value != CSS_VALUE__UNDEF) return result;

    // CSS Align 3 §7.1: legacy is a separate grammar branch for this property.
    return css_parse_legacy_justify_items(value);
}

void css_store_grid_justify_items(LayoutContext* lycon, ViewBlock* block,
                                         CssSelfAlignment value) {
    if (!block || value.value == CSS_VALUE__UNDEF) return;
    alloc_grid_prop(lycon, block);
    block->embedp()->grid->justify_items = value.value;
    block->embedp()->grid->justify_items_detail = value;
}

void resolve_grid_alignment_property(LayoutContext* lycon, ViewBlock* block,
                                            ViewSpan* span, CssPropertyCode property,
                                            const CssValue* value) {
    if (property == CSS_PROPERTY_ALIGN_SELF) {
        CssSelfAlignment self = css_parse_self_alignment_value(value, false);
        if (self.value == CSS_VALUE__UNDEF) return;
        css_store_self_alignment(lycon, span, false, self);
        CssEnum alignment = self.value;
        if (span->parent_item_kind() == DomElement::PARENT_ITEM_GRID) {
            span->gi->align_self_grid = alignment;
        } else {
            // preserve self-alignment when the parent flex role is installed after the cascade.
            alloc_flex_item_prop(lycon, span);
            span->fi->align_self = alignment;
        }
        return;
    }
    bool self = property == CSS_PROPERTY_JUSTIFY_SELF;
    if (!self && !block) {
        return;
    }
    if (self) {
        CssSelfAlignment alignment = css_parse_self_alignment_value(value, true);
        if (alignment.value == CSS_VALUE__UNDEF) return;
        css_store_self_alignment(lycon, span, true, alignment);
        if (span->parent_item_kind() == DomElement::PARENT_ITEM_GRID) {
            span->gi->justify_self = alignment.value;
        }
    } else {
        css_store_grid_justify_items(lycon, block, css_parse_justify_items(value));
    }
}

