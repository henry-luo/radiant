#include "view.hpp"
#include "render.hpp"
#include "tagged.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"

// Shared effect queries are also used by isolated stacking-order tests.
bool render_filter_effective(const FilterProp* filter) {
    return filter && filter->functions;
}

namespace radiant {
bool has_transform(DomElement* elem) {
    return elem && transform_has_functions(elem->transform_prop());
}

bool transform_preserves_3d(DomElement* elem) {
    if (!elem || elem->transformp()->transform_style != CSS_VALUE_PRESERVE_3D) return false;
    const ScrollProp* scroll = elem->scroll();
    const InlineProp* inline_prop = elem->inl();
    // grouping effects flatten descendants while preserving the authored computed keyword.
    return (scroll->overflow_x == CSS_VALUE_VISIBLE || scroll->overflow_x == CSS_VALUE_CLIP) &&
        (scroll->overflow_y == CSS_VALUE_VISIBLE || scroll->overflow_y == CSS_VALUE_CLIP) &&
        !(inline_prop->opacity >= 0.0f && inline_prop->opacity < 1.0f) &&
        (!inline_prop->mix_blend_mode || inline_prop->mix_blend_mode == CSS_VALUE_NORMAL) &&
        !elem->filterp()->functions &&
        !(elem->boundary()->mask && elem->boundary()->mask->active()) &&
        !elem->block()->containment().contain_paint && !elem->block()->containment().content_visibility_skipped;
}

bool transform_establishes_containing_block(DomElement* elem) {
    if (!elem) return false;
    const TransformProp* transform = elem->transformp();
    if (has_transform(elem) || transform->perspective > 0.0f ||
        transform->transform_style == CSS_VALUE_PRESERVE_3D) return true;
    ViewElement* parent = elem->parent_view();
    // hidden backfaces establish a containing block when the element participates in a 3D context.
    return transform->backface_visibility == CSS_VALUE_HIDDEN && parent &&
        transform_preserves_3d(parent);
}

} // namespace radiant

const char* render_clip_css_value(ViewElement* view) {
    if (!view) return nullptr;
    for (DomElement* element = lam::dom_require_element(lam::view_dom_node(view));
            element; element = dom_parent_element(element)) {
        CssDeclaration* declaration = dom_element_get_specified_value(element, CSS_PROPERTY_CLIP_PATH);
        if (!declaration || !declaration->value) return nullptr;
        if (declaration->value->type == CSS_VALUE_TYPE_KEYWORD) {
            CssEnum keyword = declaration->value->data.keyword;
            if (keyword == CSS_VALUE_INHERIT) continue;
            // CSS-wide defaults compute to none, not an unknown shape with empty coverage.
            if (keyword == CSS_VALUE_NONE || keyword == CSS_VALUE_INITIAL ||
                    keyword == CSS_VALUE_UNSET) return nullptr;
        }
        return declaration->value_text_len > 0 ? declaration->value_text : nullptr;
    }
    return nullptr;
}
