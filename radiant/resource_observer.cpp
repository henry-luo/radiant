#include "resource_observer.hpp"
#include "../lib/provider.hpp"

static const RadiantResourceObserverOps* resource_observer_ops = nullptr;

bool radiant_resource_observer_register(const RadiantResourceObserverOps* operations) {
    return lambda_provider_register(&resource_observer_ops, operations,
                                    &RadiantResourceObserverOps::image_complete);
}

void radiant_resource_image_complete(void* element, bool loaded) {
    const RadiantResourceObserverOps* operations = lambda_provider_load(&resource_observer_ops);
    if (operations) operations->image_complete(element, loaded);
}
