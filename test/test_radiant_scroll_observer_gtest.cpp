#include "test_native_provider.hpp"
#include "radiant/scroll_observer.hpp"

static void* notified_element;
static float notified_x;
static float notified_y;
static int notification_count;

static void observe_scroll(void* element, float old_x, float old_y) {
    notified_element = element;
    notified_x = old_x;
    notified_y = old_y;
    notification_count++;
}

TEST(RadiantScrollObserverTest, NativeFallbackAndHostPublication) {
    int element = 0;
    radiant_scroll_position_changed(&element, 1.5f, 2.5f);
    EXPECT_EQ(notification_count, 0);
    static const RadiantScrollObserverOps incomplete = {};
    static const RadiantScrollObserverOps host = {observe_scroll};
    static const RadiantScrollObserverOps conflict = {observe_scroll};
    ASSERT_TRUE(expect_native_provider_registration(radiant_scroll_observer_register,
                                                     &incomplete, &host, &conflict));
    radiant_scroll_position_changed(&element, 1.5f, 2.5f);
    EXPECT_EQ(notification_count, 1);
    EXPECT_EQ(notified_element, &element);
    EXPECT_FLOAT_EQ(notified_x, 1.5f);
    EXPECT_FLOAT_EQ(notified_y, 2.5f);
}
