#pragma once

struct EventContext;
struct DomDocument;
struct DomNode;
typedef DomNode View;

// Native reflow serves the event controller and the animation tree.
void layout_event_document_reflow(EventContext* context, DomDocument* document, View* iframe_container);
