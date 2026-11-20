#pragma once

struct DomElement;

struct HtmlGaugeValues {
    double value, minimum, maximum, low, high, optimum;
    bool progress;
    bool determinate;
};

// Paint and the DOM expose the same normalized HTML attribute values.
bool radiant_html_gauge_values(DomElement* element, HtmlGaugeValues* out);
double radiant_html_gauge_fraction(const HtmlGaugeValues& gauge);

// These native queries are shared with the Lambda DOM adapter.
const char* radiant_form_control_type(DomElement* element);
const char* radiant_form_button_type(DomElement* button);
bool radiant_form_control_disabled(DomElement* element);
DomElement* radiant_input_datalist(DomElement* input);
DomElement* radiant_form_owner(DomElement* control);
bool radiant_is_labelable(DomElement* element);
DomElement* radiant_label_control(DomElement* label);
DomElement* radiant_label_form(DomElement* label);
bool radiant_form_is_submit_button(DomElement* element);
bool radiant_form_is_reset_button(DomElement* element);
DomElement* radiant_form_first_submitter(DomElement* form, bool skip_disabled);
