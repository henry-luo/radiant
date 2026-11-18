#include "view.hpp"

DomElement* dom_select_option_at_index(DomElement* select, int index) {
    if (index < 0) return nullptr;
    for (DomElement* option = dom_select_next_option(select, nullptr); option;
         option = dom_select_next_option(select, option)) {
        if (index-- == 0) return option;
    }
    return nullptr;
}

void dom_option_label(DomElement* option, StrBuf* out) {
    if (!out) return;
    strbuf_reset(out);
    if (!option || option->tag() != MARKUP_NAME_OPTION) return;
    const char* label = option->get_attribute("label");
    if (label && *label) strbuf_append_str(out, label);
    else dom_option_text_normalized(option, out);
}
