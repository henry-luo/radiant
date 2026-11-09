#include "test_native_provider.hpp"
#include "radiant/scene3d/scene3d_physics_event_host.hpp"

static DomElement* observed_target;
static const PhysicsScene* observed_scene;
static const PhysicsEvent* observed_event;
static const char* observed_type;
static bool publish_result;

static bool publish(DomElement* target, const PhysicsScene* scene,
                    const PhysicsEvent* event, const char* type) {
    observed_target = target;
    observed_scene = scene;
    observed_event = event;
    observed_type = type;
    return publish_result;
}

TEST(Scene3dPhysicsEventHostTest, NativeEventsAndSynchronousHostPublication) {
    int marker = 0;
    auto* target = reinterpret_cast<DomElement*>(&marker);
    auto* scene = reinterpret_cast<const PhysicsScene*>(&marker);
    auto* event = reinterpret_cast<const PhysicsEvent*>(&marker);
    EXPECT_TRUE(scene3d_physics_event_publish(target, scene, event, "physicscontactstart"));
    static const Scene3dPhysicsEventHostOps incomplete = {nullptr};
    static const Scene3dPhysicsEventHostOps host = {publish};
    static const Scene3dPhysicsEventHostOps conflict = {publish};
    ASSERT_TRUE(expect_native_provider_registration(scene3d_physics_event_host_register,
                                                     &incomplete, &host, &conflict));
    publish_result = true;
    EXPECT_TRUE(scene3d_physics_event_publish(target, scene, event, "physicscontactstart"));
    EXPECT_EQ(observed_target, target);
    EXPECT_EQ(observed_scene, scene);
    EXPECT_EQ(observed_event, event);
    EXPECT_STREQ(observed_type, "physicscontactstart");
    publish_result = false;
    EXPECT_FALSE(scene3d_physics_event_publish(target, scene, event, "physicsjointbreak"));
}
