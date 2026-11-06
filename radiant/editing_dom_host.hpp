#pragma once

#include "../lambda/input/css/dom_element.hpp"

struct DomDocument;
struct DomElement;
struct DomNode;

// A registered script adapter can be unavailable outside an active evaluator.
struct RadiantEditingDomHostOps {
    bool (*available)();
    bool (*insert_child)(DomElement* parent, DomNode* child, DomNode* reference);
    bool (*remove_child)(DomNode* child);
    bool (*insert_html)(DomDocument* document, const char* html);
    void (*notify)(DomJsMutationKind kind, DomNode* target, DomNode* parent,
                   const char* attribute_name, const char* old_value);
};

bool radiant_editing_dom_host_register(const RadiantEditingDomHostOps* operations);
bool radiant_editing_dom_insert_child(DomElement* parent, DomNode* child, DomNode* reference,
                                      bool* host_present);
bool radiant_editing_dom_remove_child(DomNode* child, bool* host_present);
bool radiant_editing_dom_insert_html(DomDocument* document, const char* html);
// True means a script host delivered the mutation; false leaves native delivery to the caller.
bool radiant_editing_dom_notify_host(DomJsMutationKind kind, DomNode* target, DomNode* parent,
                                     const char* attribute_name, const char* old_value);
