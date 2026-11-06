#include "input_value.hpp"
#include "../lambda/input/css/dom_element.hpp"
#include "../lib/arraylist.h"
#include "../lib/mem.h"
#include "../lib/str.h"

typedef struct RadiantInputStateEntry {
    DomElement* element;
    char* value;
} RadiantInputStateEntry;

typedef struct RadiantInputState : DomDocumentResourceData {
    DomDocument* document;
    ArrayList* entries;
} RadiantInputState;

static void riv_entry_destroy(RadiantInputStateEntry* entry) {
    if (!entry) return;
    mem_free(entry->value);
    mem_free(entry);
}

static void riv_state_destroy(DomDocumentResourceData* data) {
    RadiantInputState* state = (RadiantInputState*)data;
    if (!state) return;
    if (state->entries) {
        for (int i = 0; i < state->entries->length; i++)
            riv_entry_destroy((RadiantInputStateEntry*)arraylist_get(state->entries, i));
        arraylist_free(state->entries);
    }
    mem_free(state);
}

static RadiantInputState* riv_state_get(DomDocument* document, bool create) {
    if (!document) return nullptr;
    for (DomDocumentResource* resource = document->resources; resource; resource = resource->next) {
        if (resource->destroy == riv_state_destroy) return (RadiantInputState*)resource->data;
    }
    if (!create) return nullptr;
    RadiantInputState* state = (RadiantInputState*)mem_calloc(
        1, sizeof(RadiantInputState), MEM_CAT_LAYOUT);
    if (!state) return nullptr;
    state->document = document;
    state->entries = arraylist_new(8);
    if (!state->entries || !dom_document_add_resource(document, state, riv_state_destroy)) {
        if (state->entries) arraylist_free(state->entries);
        mem_free(state);
        return nullptr;
    }
    return state;
}

static RadiantInputStateEntry* riv_entry_get(DomElement* element, bool create) {
    if (!element) return nullptr;
    RadiantInputState* state = riv_state_get(element->doc, create);
    if (!state) return nullptr;
    for (int i = 0; i < state->entries->length; i++) {
        RadiantInputStateEntry* entry =
            (RadiantInputStateEntry*)arraylist_get(state->entries, i);
        if (entry && entry->element == element) return entry;
    }
    if (!create) return nullptr;
    RadiantInputStateEntry* entry = (RadiantInputStateEntry*)mem_calloc(
        1, sizeof(RadiantInputStateEntry), MEM_CAT_LAYOUT);
    if (!entry) return nullptr;
    entry->element = element;
    const char* type = element->get_attribute("type");
    const char* initial = element->get_attribute("value");
    char sanitized[128];
    radiant_input_value_sanitize(type, initial ? initial : "", sanitized, sizeof(sanitized));
    entry->value = mem_strdup(sanitized, MEM_CAT_LAYOUT);
    if (!arraylist_append(state->entries, entry)) {
        riv_entry_destroy(entry);
        return nullptr;
    }
    return entry;
}

// Preserve the initialization side effect when the script adapter first accesses files.
extern "C" bool radiant_input_ensure_live_value(DomElement* element) {
    return riv_entry_get(element, true) != nullptr;
}

extern "C" const char* radiant_input_live_value(DomElement* element) {
    RadiantInputStateEntry* entry = riv_entry_get(element, true);
    return entry && entry->value ? entry->value : "";
}

// layout reads an existing live value without changing untouched HTML defaults.
extern "C" const char* radiant_input_peek_live_value(DomElement* element) {
    RadiantInputStateEntry* entry = riv_entry_get(element, false);
    return entry ? entry->value : nullptr;
}

extern "C" bool radiant_input_set_live_value(DomElement* element, const char* value) {
    RadiantInputStateEntry* entry = riv_entry_get(element, true);
    if (!entry) return false;
    const char* type = element->get_attribute("type");
    char sanitized[128];
    if (!radiant_input_value_sanitize(type, value ? value : "", sanitized,
                                      sizeof(sanitized))) return false;
    char* replacement = mem_strdup(sanitized, MEM_CAT_LAYOUT);
    if (!replacement) return false;
    mem_free(entry->value);
    entry->value = replacement;
    return true;
}

extern "C" void radiant_input_reset_live_value(DomElement* element) {
    if (!element) return;
    radiant_input_set_live_value(element,
        element->get_attribute("value"));
}

extern "C" void radiant_input_type_changed(DomElement* element) {
    RadiantInputStateEntry* entry = riv_entry_get(element, false);
    if (!entry) return;
    // Changing type re-runs the new state's sanitizer over the live value.
    radiant_input_set_live_value(element, entry->value ? entry->value : "");
}

