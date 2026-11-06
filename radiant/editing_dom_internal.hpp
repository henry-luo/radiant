#pragma once

#include "event.hpp"

// Validate a selection boundary against the editing host's live subtree.
bool radiant_editing_dom_host_contains_boundary(DomElement* host, DomBoundary boundary);

// Capture native state before removal; a false result forbids the mutation.
bool dom_edit_defer_native_retirement(DomDocument* document, DomNode* node, bool* deferred);
bool dom_edit_snapshot_native_select(DomDocument* document, DomElement* select);
