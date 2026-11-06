#pragma once

struct DomElement;
struct DomNode;

// Restore authored children before changing a parent containing table fixups.
DomElement* radiant_dom_prepare_children_mutation(DomElement* parent, DomNode* changed_child);
