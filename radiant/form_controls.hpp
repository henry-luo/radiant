#pragma once

struct DomElement;

// These native queries are shared with the Lambda DOM adapter.
const char* radiant_form_control_type(DomElement* element);
bool radiant_form_control_disabled(DomElement* element);
DomElement* radiant_form_owner(DomElement* control);
bool radiant_form_is_submit_button(DomElement* element);
bool radiant_form_is_reset_button(DomElement* element);
DomElement* radiant_form_first_submitter(DomElement* form, bool skip_disabled);
