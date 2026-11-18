#include "form_controls.hpp"
#include "lambda/core/well_known_markup_names.h"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/html_input.hpp"
#include "lib/str.h"
#include <math.h>
#include <string.h>

static double gauge_number(DomElement* element, const char* name, double fallback) {
    const char* start = dom_element_attribute_value_ns(element, "", name);
    if (!start) return fallback;
    while (*start && strchr(" \t\n\r\f", *start)) start++;
    const char* end = start;
    if (*end == '-' || *end == '+') end++;
    const char* significand = end;
    while (*end >= '0' && *end <= '9') end++;
    if (*end == '.') {
        end++;
        while (*end >= '0' && *end <= '9') end++;
    }
    if (end == significand || (end == significand + 1 && *significand == '.')) return fallback;
    // HTML reads a decimal prefix and ignores an incomplete exponent.
    if (*end == 'e' || *end == 'E') {
        const char* digits = end + 1;
        if (*digits == '-' || *digits == '+') digits++;
        const char* exponent_end = digits;
        while (*exponent_end >= '0' && *exponent_end <= '9') exponent_end++;
        if (exponent_end != digits) end = exponent_end;
    }
    double number = 0.0;
    return str_to_double(start, end - start, &number, nullptr) && isfinite(number) ? number : fallback;
}

bool radiant_html_gauge_values(DomElement* element, HtmlGaugeValues* out) {
    if (!element || !out) return false;
    bool progress = dom_element_is_html_tag(element, MARKUP_NAME_PROGRESS);
    if (!progress && !dom_element_is_html_tag(element, MARKUP_NAME_METER)) return false;
    HtmlGaugeValues gauge = {};
    gauge.progress = progress;
    gauge.determinate = !progress || dom_element_attribute_value_ns(element, "", "value") != nullptr;
    gauge.maximum = gauge_number(element, "max", 1.0);
    if (progress) {
        if (gauge.maximum <= 0.0) gauge.maximum = 1.0;
    } else {
        gauge.minimum = gauge_number(element, "min", 0.0);
        gauge.maximum = fmax(gauge.minimum, gauge.maximum);
        gauge.low = fmin(gauge.maximum, fmax(gauge.minimum, gauge_number(element, "low", gauge.minimum)));
        gauge.high = fmin(gauge.maximum, fmax(gauge.low, gauge_number(element, "high", gauge.maximum)));
        double midpoint = 0.5 * gauge.minimum + 0.5 * gauge.maximum;
        gauge.optimum = fmin(gauge.maximum, fmax(gauge.minimum, gauge_number(element, "optimum", midpoint)));
    }
    gauge.value = fmin(gauge.maximum, fmax(gauge.minimum, gauge_number(element, "value", 0.0)));
    *out = gauge;
    return true;
}

double radiant_html_gauge_fraction(const HtmlGaugeValues& gauge) {
    if (!gauge.determinate) return -1.0;
    double extent = gauge.maximum - gauge.minimum;
    if (extent <= 0.0) return 0.0;
    if (isfinite(extent)) return (gauge.value - gauge.minimum) / extent;
    // Scaling avoids overflow when a meter spans both ends of the double range.
    return (0.5 * gauge.value - 0.5 * gauge.minimum) /
           (0.5 * gauge.maximum - 0.5 * gauge.minimum);
}

const char* radiant_form_control_type(DomElement* element) {
    static __thread char buffer[24];
    const char* raw = element ? element->get_attribute("type") : nullptr;
    if (!raw || !*raw) return "text";
    size_t length = str_copy(buffer, sizeof(buffer), raw, strlen(raw));
    str_lower_inplace(buffer, length);
    return buffer;
}

bool radiant_form_control_disabled(DomElement* element) {
    return element && element->has_attribute("disabled");
}

DomElement* radiant_input_datalist(DomElement* input) {
    if (!dom_element_is_html_tag(input, MARKUP_NAME_INPUT)) return nullptr;
    const char* id = dom_element_attribute_value_ns(input, "", "list");
    if (!id || !*id) return nullptr;
    FormInputKind kind = form_input_kind(dom_element_attribute_value_ns(input, "", "type"));
    if (!HTML_INPUT_TYPES[kind].uses_datalist) return nullptr;
    // Resolve exactly the first matching ID in the input's own source tree.
    DomElement* match = dom_find_source_element_by_id(dom_source_tree_root(input), id);
    return dom_element_is_html_tag(match, MARKUP_NAME_DATALIST) ? match : nullptr;
}

DomElement* radiant_form_owner(DomElement* control) {
    if (!control) return nullptr;
    const char* form_id = dom_element_attribute_value_ns(control, "", "form");
    if (form_id && dom_node_is_connected(control)) {
        DomElement* candidate = dom_find_source_element_by_id(dom_source_tree_root(control), form_id);
        return dom_element_is_html_tag(candidate, MARKUP_NAME_FORM) ? candidate : nullptr;
    }
    for (DomNode* node = dom_source_parent(control); node; node = dom_source_parent(node)) {
        if (dom_element_is_html_tag(node->as_element(), MARKUP_NAME_FORM)) return node->as_element();
    }
    return nullptr;
}

bool radiant_is_labelable(DomElement* element) {
    switch (dom_element_html_tag(element)) {
    case MARKUP_NAME_INPUT:
        return form_input_kind(dom_element_attribute_value_ns(element, "", "type")) != FORM_INPUT_KIND_HIDDEN;
    case MARKUP_NAME_BUTTON: case MARKUP_NAME_METER: case MARKUP_NAME_OUTPUT:
    case MARKUP_NAME_PROGRESS: case MARKUP_NAME_SELECT: case MARKUP_NAME_TEXTAREA:
        return true;
    default:
        return false;
    }
}

DomElement* radiant_label_control(DomElement* label) {
    if (!dom_element_is_html_tag(label, MARKUP_NAME_LABEL)) return nullptr;
    const char* id = dom_element_attribute_value_ns(label, "", "for");
    if (id) {
        DomElement* candidate = dom_find_source_element_by_id(dom_source_tree_root(label), id);
        return radiant_is_labelable(candidate) ? candidate : nullptr;
    }
    for (DomNode* node = dom_source_next_in_tree(label, label); node;
         node = dom_source_next_in_tree(label, node)) {
        DomElement* element = node->as_element();
        if (radiant_is_labelable(element)) return element;
    }
    return nullptr;
}

DomElement* radiant_label_form(DomElement* label) {
    DomElement* control = radiant_label_control(label);
    if (dom_element_is_html_tag(control, MARKUP_NAME_METER) ||
        dom_element_is_html_tag(control, MARKUP_NAME_PROGRESS)) return nullptr;
    return radiant_form_owner(control);
}

bool radiant_form_is_submit_button(DomElement* element) {
    if (!element || !element->tag_name) return false;
    if (str_icmp_cstr(element->tag_name, "input") == 0) {
        const char* type = radiant_form_control_type(element);
        return strcmp(type, "submit") == 0 || strcmp(type, "image") == 0;
    }
    if (str_icmp_cstr(element->tag_name, "button") == 0) {
        const char* type = radiant_form_control_type(element);
        return strcmp(type, "text") == 0 || strcmp(type, "submit") == 0;
    }
    return false;
}

bool radiant_form_is_reset_button(DomElement* element) {
    if (!element || !element->tag_name) return false;
    if (str_icmp_cstr(element->tag_name, "input") != 0 &&
            str_icmp_cstr(element->tag_name, "button") != 0) return false;
    return strcmp(radiant_form_control_type(element), "reset") == 0;
}

static DomElement* form_first_submitter_in_tree(DomNode* node, DomElement* form,
                                                bool skip_disabled) {
    for (DomNode* current = node; current; current = current->next_sibling) {
        if (!current->is_element()) continue;
        DomElement* element = current->as_element();
        if (radiant_form_is_submit_button(element) && radiant_form_owner(element) == form &&
                (!skip_disabled || !radiant_form_control_disabled(element))) return element;
        DomElement* nested = form_first_submitter_in_tree(element->first_child, form, skip_disabled);
        if (nested) return nested;
    }
    return nullptr;
}

DomElement* radiant_form_first_submitter(DomElement* form, bool skip_disabled) {
    if (!form) return nullptr;
    // External controls are considered in the form's own tree order.
    DomNode* start = form->first_child;
    if (DomElement* shadow = dom_shadow_root(form)) start = shadow->first_child;
    else if (dom_node_is_connected(form)) start = form->doc->root;
    return form_first_submitter_in_tree(start, form, skip_disabled);
}
