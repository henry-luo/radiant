#pragma once

struct DomElement;
struct DomNode;

typedef void (*RadiantSelectOptionVisitor)(DomElement* option, void* context);

// Walk the options owned by a select, excluding nested select and optgroup trees.
void radiant_select_walk_options(DomNode* first, RadiantSelectOptionVisitor visitor, void* context);

struct RadiantSelectResetOps {
    bool (*selected)(DomElement* option, void* context);
    void (*set_selected)(DomElement* option, bool selected, void* context);
    void* context;
};

// Returns false when there is no single-select reset to perform.
bool radiant_select_reset(DomElement* select, const RadiantSelectResetOps* operations,
                          int* selected_index, int* option_count);

// Keep the native form control in sync after the option list changes.
void radiant_select_sync_native_index(DomElement* select, int selected_index, int option_count);
