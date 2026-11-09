#include "test_native_provider.hpp"
#include "radiant/event_sim_controller_host.hpp"

static DomNode* observed;
static bool pointer(UiContext*, DomNode* target, const char*, double, double, int, int, int, const char*) { observed = target; return true; }
static bool touch(UiContext*, DomNode*, const char*, double, double, int, bool, double) { return true; }
static bool select_change(UiContext*, DomNode*, int) { return true; }
static bool drag(UiContext*, DomNode*, uint32_t, uint32_t, DomNode*, uint32_t, uint32_t, const char*, const char*, bool) { return true; }

TEST(RadiantEventSimControllerHostTest, RoutesInputOnlyWhenControllerIsRegistered) {
    int marker = 0;
    auto* target = reinterpret_cast<DomNode*>(&marker);
    EXPECT_FALSE(radiant_event_sim_controller_pointer(nullptr, target, "down", 0, 0, 0, 0, 0, "mouse"));
    static const RadiantEventSimControllerHostOps incomplete = {};
    static const RadiantEventSimControllerHostOps host = {pointer, touch, select_change, drag};
    static const RadiantEventSimControllerHostOps conflict = {pointer, touch, select_change, drag};
    ASSERT_TRUE(expect_native_provider_registration(radiant_event_sim_controller_host_register, &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_event_sim_controller_pointer(nullptr, target, "down", 0, 0, 0, 0, 0, "mouse"));
    EXPECT_EQ(observed, target);
}
