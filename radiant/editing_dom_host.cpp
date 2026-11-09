#include "editing_dom_host.hpp"
#include "lib/provider.hpp"

static const RadiantEditingDomHostOps* editing_dom_host_ops = nullptr;

bool radiant_editing_dom_host_register(const RadiantEditingDomHostOps* operations) {
    return lambda_provider_register(&editing_dom_host_ops, operations,
        &RadiantEditingDomHostOps::available,
        &RadiantEditingDomHostOps::insert_child,
        &RadiantEditingDomHostOps::remove_child, &RadiantEditingDomHostOps::insert_html,
        &RadiantEditingDomHostOps::notify);
}

bool radiant_editing_dom_insert_child(DomElement* parent, DomNode* child, DomNode* reference,
                                      bool* host_present) {
    const RadiantEditingDomHostOps* operations = lambda_provider_load(&editing_dom_host_ops);
    bool available = operations && operations->available();
    if (host_present) *host_present = available;
    return available && operations->insert_child(parent, child, reference);
}

bool radiant_editing_dom_remove_child(DomNode* child, bool* host_present) {
    const RadiantEditingDomHostOps* operations = lambda_provider_load(&editing_dom_host_ops);
    bool available = operations && operations->available();
    if (host_present) *host_present = available;
    return available && operations->remove_child(child);
}

bool radiant_editing_dom_insert_html(DomDocument* document, const char* html) {
    const RadiantEditingDomHostOps* operations = lambda_provider_load(&editing_dom_host_ops);
    return operations && operations->available() && operations->insert_html(document, html);
}

bool radiant_editing_dom_notify_host(DomJsMutationKind kind, DomNode* target, DomNode* parent,
                                     const char* attribute_name, const char* old_value) {
    const RadiantEditingDomHostOps* operations = lambda_provider_load(&editing_dom_host_ops);
    if (!operations || !operations->available()) return false;
    operations->notify(kind, target, parent, attribute_name, old_value);
    return true;
}
