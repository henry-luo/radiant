#pragma once

#include <stdint.h>
struct DomDocument;
struct DomElement;
enum DomScrollAlign : uint8_t;
enum DomScrollBehavior : uint8_t;

// A queued target is pinned until layout consumes or replaces the request.
void radiant_queue_scroll_into_view(DomDocument* document, DomElement* element, bool center,
                                   bool if_needed, DomScrollAlign block, DomScrollAlign inline_align,
                                   DomScrollBehavior behavior);
