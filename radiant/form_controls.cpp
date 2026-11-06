#include "form_controls.hpp"
#include "../lambda/input/css/dom_element.hpp"
#include "../lib/str.h"
#include <string.h>

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

DomElement* radiant_form_owner(DomElement* control) {
    if (!control) return nullptr;
    const char* form_id = control->get_attribute("form");
    if (form_id && *form_id) {
        DomDocument* document = control->doc;
        return document && document->root ? dom_find_element_by_id(document->root, form_id) : nullptr;
    }
    for (DomNode* node = control->parent; node && node->is_element(); node = node->parent) {
        DomElement* element = node->as_element();
        if (element->tag_name && str_icmp_cstr(element->tag_name, "form") == 0) return element;
    }
    return nullptr;
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
    // Connected external controls participate in document order; detached forms use their subtree.
    DomNode* start = dom_node_is_connected(form) ? static_cast<DomNode*>(form->doc->root.get())
                                                : form->first_child.get();
    return form_first_submitter_in_tree(start, form, skip_disabled);
}
