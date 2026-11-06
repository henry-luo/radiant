#pragma once

// Optional host notification for completed asynchronous image loads.
struct RadiantResourceObserverOps {
    void (*image_complete)(void* element, bool loaded);
};

bool radiant_resource_observer_register(const RadiantResourceObserverOps* operations);
void radiant_resource_image_complete(void* element, bool loaded);
