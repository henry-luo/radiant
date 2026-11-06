#pragma once

struct DocState;
struct DomNode;

// Relocate native interaction state before unlinking a subtree.
void radiant_dom_pre_remove_state(DocState* state, DomNode* child);
