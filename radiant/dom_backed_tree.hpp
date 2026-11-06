#pragma once

struct DomElement;
struct DomNode;
struct DomText;

// A script DOM host can supply its existing preparation and removal semantics.
// These backing operations do not publish mutation records or adjust live ranges.
struct RadiantDomBackedTreeOps {
    DomElement* (*prepare)(DomElement* parent, DomNode* changed_child);
    bool (*remove)(DomElement* parent, DomNode* child);
};

struct RadiantDomTreeMutationOps {
    RadiantDomBackedTreeOps backing;
    void (*pre_remove_for_move)(DomElement* parent, DomNode* child);
    void (*post_insert)(DomNode* parent, DomNode* child);
    void (*notify_insert)(DomNode* parent, DomNode* child);
};

bool radiant_dom_insert_backed_element(DomElement* parent, DomNode* child, DomNode* reference,
                                       const RadiantDomBackedTreeOps* operations);
bool radiant_dom_insert_backed_text(DomElement* parent, DomText* text, DomNode* reference,
                                    const RadiantDomBackedTreeOps* operations);
bool radiant_dom_insert_before_child(DomElement* parent, DomNode* child, DomNode* reference,
                                     const RadiantDomTreeMutationOps* operations);
// Removal retires detached resources only after the active edit commits.
bool radiant_dom_remove_edit_child(DomNode* child);
