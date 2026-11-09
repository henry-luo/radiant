#include "event.hpp"
#include "event_host.hpp"
#include <string.h>

void rdt_event_set_mouse_position(RdtEvent* event, EventType type,
                                  float x, float y, double timestamp) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->mouse_position.type = type;
    event->mouse_position.timestamp = timestamp;
    event->mouse_position.x = x;
    event->mouse_position.y = y;
}

void rdt_event_set_mouse_button(RdtEvent* event, EventType type,
                                float x, float y, int button, int clicks,
                                int mods, double timestamp) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->mouse_button.type = type;
    event->mouse_button.timestamp = timestamp;
    event->mouse_button.x = x;
    event->mouse_button.y = y;
    event->mouse_button.button = (uint8_t)button;
    event->mouse_button.clicks = (uint8_t)clicks;
    event->mouse_button.mods = mods;
}

void rdt_event_set_scroll(RdtEvent* event, float x, float y,
                          float xoffset, float yoffset, double timestamp) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->scroll.type = RDT_EVENT_SCROLL;
    event->scroll.timestamp = timestamp;
    event->scroll.x = x;
    event->scroll.y = y;
    event->scroll.xoffset = xoffset;
    event->scroll.yoffset = yoffset;
}

void rdt_event_set_key(RdtEvent* event, EventType type, int key, int mods,
                       int scancode, double timestamp) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->key.type = type;
    event->key.timestamp = timestamp;
    event->key.key = key;
    event->key.scancode = scancode;
    event->key.mods = mods;
}

void rdt_event_set_text_input(RdtEvent* event, uint32_t codepoint,
                              double timestamp) {
    if (!event) return;
    memset(event, 0, sizeof(*event));
    event->text_input.type = RDT_EVENT_TEXT_INPUT;
    event->text_input.timestamp = timestamp;
    event->text_input.codepoint = codepoint;
}


DomElement* radiant_document_body_element(DomDocument* doc) {
    return dom_document_body_element(doc);
}

bool radiant_event_host_ime_composition(UiContext* ui, EventType type,
                                        const char* text, uint32_t caret) {
    RdtEvent event = {};
    event.composition.type = type;
    event.composition.text = text;
    event.composition.preedit_caret = caret;
    return radiant_event_host_composition(ui, &event);
}
