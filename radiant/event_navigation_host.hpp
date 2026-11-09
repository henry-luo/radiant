#pragma once

struct UiContext;
struct DomDocument;
struct DomElement;

// Deferred navigation and document-load decisions belong to the event controller.
struct RadiantEventNavigationHostOps {
    bool (*commit_navigation)(UiContext* ui);
    bool (*poll_loads)(UiContext* ui);
    void (*drain_loads)(UiContext* ui);
    bool (*pending_loads)(UiContext* ui);
    bool (*commit_loads)(UiContext* ui);
    void (*cancel_loads)(DomDocument* document);
    bool (*navigate)(UiContext* ui, DomDocument* document, const char* url);
    bool (*schedule_iframe)(UiContext* ui, DomElement* iframe, const char* url, int width, int height);
};

bool radiant_event_navigation_host_register(const RadiantEventNavigationHostOps* operations);
bool radiant_event_navigation_commit(UiContext* ui);
bool radiant_event_navigation_poll_loads(UiContext* ui);
void radiant_event_navigation_drain_loads(UiContext* ui);
bool radiant_event_navigation_pending_loads(UiContext* ui);
bool radiant_event_navigation_commit_loads(UiContext* ui);
void radiant_event_navigation_cancel_loads(DomDocument* document);
bool radiant_event_navigation_location(UiContext* ui, DomDocument* document, const char* url);
bool radiant_event_navigation_schedule_iframe(UiContext* ui, DomElement* iframe, const char* url, int width, int height);
