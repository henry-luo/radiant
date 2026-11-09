#include "view.hpp"
#include "lambda/input/css/css_style.hpp"

bool css_property_runtime_inherited(CssPropertyCode property) {
    if (css_property_is_svg_presentation(property) && !css_property_is_svg_paint(property)) {
        const CssProperty* metadata = css_property_get_by_code(property);
        return metadata && metadata->inheritance == PROP_INHERIT_YES;
    }
    static const CssPropertyCode resolver_inherited[] = {
        CSS_PROPERTY_FONT_FAMILY, CSS_PROPERTY_FONT_SIZE,
        CSS_PROPERTY_FONT_WEIGHT, CSS_PROPERTY_FONT_STYLE,
        CSS_PROPERTY_FONT_VARIANT, CSS_PROPERTY_COLOR,
        CSS_PROPERTY_LINE_HEIGHT, CSS_PROPERTY_TEXT_ALIGN,
        CSS_PROPERTY_TEXT_DECORATION, CSS_PROPERTY_TEXT_EMPHASIS,
        CSS_PROPERTY_TEXT_EMPHASIS_STYLE, CSS_PROPERTY_TEXT_EMPHASIS_POSITION,
        CSS_PROPERTY_TEXT_TRANSFORM, CSS_PROPERTY_TEXT_INDENT,
        CSS_PROPERTY_TEXT_SPACING_TRIM, CSS_PROPERTY_HYPHENATE_CHARACTER,
        CSS_PROPERTY_HYPHENATE_LIMIT_CHARS,
        CSS_PROPERTY_DOMINANT_BASELINE, CSS_PROPERTY_LETTER_SPACING,
        CSS_PROPERTY_WORD_SPACING, CSS_PROPERTY_WHITE_SPACE,
        CSS_PROPERTY_TAB_SIZE,
        CSS_PROPERTY_FILL, CSS_PROPERTY_STROKE, CSS_PROPERTY_STROKE_WIDTH,
        CSS_PROPERTY_ACCENT_COLOR, CSS_PROPERTY_VISIBILITY,
        // native hit testing consumes the same inherited cursor as computed style.
        CSS_PROPERTY_CURSOR,
        CSS_PROPERTY_EMPTY_CELLS, CSS_PROPERTY_DIRECTION,
        CSS_PROPERTY_LIST_STYLE_POSITION, CSS_PROPERTY_LIST_STYLE_TYPE,
        CSS_PROPERTY_LIST_STYLE, CSS_PROPERTY_RUBY_POSITION,
        CSS_PROPERTY_IMAGE_RENDERING
    };
    for (size_t i = 0; i < sizeof(resolver_inherited) / sizeof(resolver_inherited[0]); i++) {
        if (resolver_inherited[i] == property) return true;
    }
    return false;
}

const CssPropertyCode* css_property_runtime_inherited_list(size_t* count) {
    struct InheritedProperties {
        CssPropertyCode values[CSS_PROPERTY_COUNT];
        size_t count = 0;
        InheritedProperties() {
            for (int code = CSS_PROPERTY_DISPLAY; code < CSS_PROPERTY_COUNT; code++) {
                CssPropertyCode property = (CssPropertyCode)code;
                if (css_property_runtime_inherited(property)) values[count++] = property;
            }
        }
    };
    // the property database is immutable; enumerate its inheritance policy once, not per element.
    static const InheritedProperties properties;
    *count = properties.count;
    return properties.values;
}

