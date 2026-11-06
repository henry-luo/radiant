#pragma once

#include "../lambda/core/render_bridge.hpp"

struct DomElement;

// The host owns render-map Items and supplies them only for the active rebuild.
struct RadiantReactiveRenderHostOps {
    Item (*document_root)();
    Item (*result)(Item source, const char* template_ref);
    bool (*equal)(Item left, Item right);
    bool (*set_metadata)(DomElement* element, const char* name, const char* value);
};

bool radiant_reactive_render_host_register(const RadiantReactiveRenderHostOps* operations);
bool radiant_reactive_render_host_available();
Item radiant_reactive_render_document_root();
Item radiant_reactive_render_result(Item source, const char* template_ref);
bool radiant_reactive_render_equal(Item left, Item right);
bool radiant_reactive_render_set_metadata(DomElement* element, const char* name, const char* value);
