#pragma once

struct DomElement;

typedef void (*RadiantSelectOptionVisitor)(DomElement* option, void* context);

DomElement* dom_select_next_option(DomElement* select, DomElement* previous);
DomElement* dom_select_option_owner(DomElement* option);
void radiant_select_walk_options(DomElement* select, RadiantSelectOptionVisitor visitor, void* context);

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
