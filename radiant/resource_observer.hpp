#pragma once

// optional host notification for completed asynchronous element resources.
struct RadiantResourceObserverOps {
    // legacy field name retained for the one-pointer provider ABI.
    void (*image_complete)(void* element, bool loaded);
};

bool radiant_resource_observer_register(const RadiantResourceObserverOps* operations);
void radiant_resource_complete(void* element, bool loaded);
void radiant_resource_image_complete(void* element, bool loaded);
