#pragma once

#include <gtest/gtest.h>

// Each executable owns its provider registration, so absence is observable.
template<typename Operations>
bool expect_native_provider_registration(bool (*register_provider)(const Operations*),
                                         const Operations* incomplete,
                                         const Operations* host,
                                         const Operations* conflict) {
    EXPECT_FALSE(register_provider(nullptr));
    EXPECT_FALSE(register_provider(incomplete));
    bool registered = register_provider(host);
    EXPECT_TRUE(registered);
    if (!registered) return false;
    EXPECT_TRUE(register_provider(host));
    EXPECT_FALSE(register_provider(conflict));
    return true;
}
