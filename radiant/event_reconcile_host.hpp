#pragma once

struct UiContext;
struct DomDocument;
struct DomElement;
struct DomNode;

// Event-owned mutation and rebuild work runs in the registered controller.
struct RadiantEventReconcileHostOps {
    bool (*apply_load_cascade)(DomDocument*, const char**);
    bool (*prepare_layout)(DomDocument*, const char**);
    bool (*pending_templates)(UiContext*, DomDocument*);
    void (*mutations)(UiContext*, DomDocument*);
    void (*attribute)(DomElement*, const char*, const char*);
    void (*render_events)(DomDocument*, DomNode*);
    void (*follow_rebuild)(DomDocument*, DomNode*, DomNode*);
};

bool radiant_event_reconcile_host_register(const RadiantEventReconcileHostOps* operations);
bool radiant_event_reconcile_apply_load(DomDocument* document, const char** reason);
bool radiant_event_reconcile_prepare_layout(DomDocument* document, const char** reason);
bool radiant_event_reconcile_templates(UiContext* ui, DomDocument* document);
void radiant_event_reconcile_mutations(UiContext* ui, DomDocument* document);
void radiant_event_reconcile_attribute(DomElement* element, const char* name, const char* old_value);
void radiant_event_reconcile_render_events(DomDocument* document, DomNode* root);
void radiant_event_reconcile_follow_rebuild(DomDocument* document, DomNode* old_root, DomNode* new_root);
