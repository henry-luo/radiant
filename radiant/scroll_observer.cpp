#include "scroll_observer.hpp"
#include "../lib/provider.hpp"

static const RadiantScrollObserverOps* scroll_observer_ops = nullptr;

bool radiant_scroll_observer_register(const RadiantScrollObserverOps* operations) {
    return lambda_provider_register(&scroll_observer_ops, operations,
                                    &RadiantScrollObserverOps::position_changed);
}

void radiant_scroll_position_changed(void* element, float old_x, float old_y) {
    const RadiantScrollObserverOps* operations = lambda_provider_load(&scroll_observer_ops);
    if (operations) operations->position_changed(element, old_x, old_y);
}
