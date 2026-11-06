#pragma once

// The optional script host receives native scroll changes for event delivery.
struct RadiantScrollObserverOps {
    void (*position_changed)(void* element, float old_x, float old_y);
};

bool radiant_scroll_observer_register(const RadiantScrollObserverOps* operations);
void radiant_scroll_position_changed(void* element, float old_x, float old_y);
