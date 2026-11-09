#include "test_native_provider.hpp"
#include "radiant/resource_observer.hpp"

static void* notified_element;
static bool notified_loaded;
static int notifications;

static void image_complete(void* element, bool loaded) {
    notified_element = element;
    notified_loaded = loaded;
    notifications++;
}

TEST(RadiantResourceObserverTest, NativeFallbackAndHostPublication) {
    int element = 0;
    radiant_resource_image_complete(&element, true);
    EXPECT_EQ(notifications, 0);
    static const RadiantResourceObserverOps incomplete = {};
    static const RadiantResourceObserverOps host = {image_complete};
    static const RadiantResourceObserverOps conflict = {image_complete};
    ASSERT_TRUE(expect_native_provider_registration(radiant_resource_observer_register,
                                                     &incomplete, &host, &conflict));
    radiant_resource_image_complete(&element, true);
    EXPECT_EQ(notifications, 1);
    EXPECT_EQ(notified_element, &element);
    EXPECT_TRUE(notified_loaded);
    radiant_resource_image_complete(&element, false);
    EXPECT_EQ(notifications, 2);
    EXPECT_FALSE(notified_loaded);
}
