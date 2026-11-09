#include "test_native_provider.hpp"
#include "radiant/scene3d/scene3d_navigation_event_host.hpp"

static DomElement* observed_target;
static Scene3dNavigation* observed_state;
static Scene3dNavigationAgent* observed_agent;
static const char* observed_type;
static bool delivery_succeeds = true;

static bool publish(DomElement* target, Scene3dNavigation* state,
                    Scene3dNavigationAgent* agent, const char* type) {
    observed_target = target;
    observed_state = state;
    observed_agent = agent;
    observed_type = type;
    return delivery_succeeds;
}

TEST(Scene3dNavigationEventHostTest, NativeAndRegisteredDelivery) {
    int marker = 0;
    auto* target = reinterpret_cast<DomElement*>(&marker);
    auto* state = reinterpret_cast<Scene3dNavigation*>(&marker);
    auto* agent = reinterpret_cast<Scene3dNavigationAgent*>(&marker);
    EXPECT_TRUE(scene3d_navigation_event_publish(target, state, agent, "path"));
    EXPECT_EQ(observed_target, nullptr);
    static const Scene3dNavigationEventHostOps incomplete = {nullptr};
    static const Scene3dNavigationEventHostOps host = {publish};
    static const Scene3dNavigationEventHostOps conflict = {publish};
    ASSERT_TRUE(expect_native_provider_registration(scene3d_navigation_event_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(scene3d_navigation_event_publish(target, state, agent, "path"));
    EXPECT_EQ(observed_target, target);
    EXPECT_EQ(observed_state, state);
    EXPECT_EQ(observed_agent, agent);
    EXPECT_STREQ(observed_type, "path");
    delivery_succeeds = false;
    EXPECT_FALSE(scene3d_navigation_event_publish(target, state, agent, "path"));
}
