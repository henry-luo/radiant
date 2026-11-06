#pragma once

#include "../lib/arraylist.h"
struct DomElement;
struct DomNode;

// Table layout and mutation share these layout-only sibling operations.
void layout_table_append_detached_node(DomElement* parent, DomNode* child);
void layout_table_insert_node_before(DomElement* parent, DomNode* node, DomNode* reference);
ArrayList* layout_table_snapshot_children(DomElement* parent);
void layout_table_unwrap_fixup(DomElement* fixup, DomElement* parent, DomNode* before);
