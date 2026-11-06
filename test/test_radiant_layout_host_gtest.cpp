#include "test_native_provider.hpp"
#include "../radiant/layout_host.hpp"

static int suspends;
static int resumes;
static int posts;
static int restores;
static int completions;
static DomDocument* captured_parent;
static DomElement* completed_frame;
static bool suspend_watchdog() { suspends++; return true; }
static void resume_watchdog() { resumes++; }
static void post_layout() { posts++; }
static void* capture_iframe_context(DomDocument* parent) { captured_parent = parent; return parent; }
static void restore_iframe_context(UiContext*, DomDocument* parent, void* context) {
    if (parent == context) restores++;
}
static bool defer_iframe_load() { return true; }
static void iframe_load_complete(DomDocument*, DomElement* frame) { completed_frame = frame; completions++; }

static float geometry_fallback_dimension(DomElement*, bool width_axis) {
    return width_axis ? 23.0f : 17.0f;
}

TEST(RadiantLayoutHostTest, NativeFallbackAndHostPublication) {
    EXPECT_FALSE(radiant_layout_suspend_watchdog());
    radiant_layout_resume_watchdog();
    radiant_layout_post_observers();
    EXPECT_EQ(suspends + resumes + posts, 0);
    EXPECT_EQ(radiant_layout_capture_iframe_context(nullptr), nullptr);
    EXPECT_FALSE(radiant_layout_defer_iframe_load());
    EXPECT_FLOAT_EQ(radiant_layout_geometry_fallback_dimension(nullptr, true), 0.0f);
    radiant_layout_iframe_load_complete(nullptr, nullptr);
    static const RadiantLayoutHostOps incomplete = {suspend_watchdog, resume_watchdog, post_layout,
        capture_iframe_context, restore_iframe_context, defer_iframe_load, iframe_load_complete, nullptr};
    static const RadiantLayoutHostOps host = {suspend_watchdog, resume_watchdog, post_layout,
        capture_iframe_context, restore_iframe_context, defer_iframe_load, iframe_load_complete, geometry_fallback_dimension};
    static const RadiantLayoutHostOps conflict = {suspend_watchdog, resume_watchdog, post_layout,
        capture_iframe_context, restore_iframe_context, defer_iframe_load, iframe_load_complete, geometry_fallback_dimension};
    ASSERT_TRUE(expect_native_provider_registration(radiant_layout_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_layout_suspend_watchdog());
    radiant_layout_resume_watchdog();
    radiant_layout_post_observers();
    EXPECT_EQ(suspends, 1);
    EXPECT_EQ(resumes, 1);
    EXPECT_EQ(posts, 1);
    int marker = 0;
    DomDocument* parent = reinterpret_cast<DomDocument*>(&marker);
    DomElement* frame = reinterpret_cast<DomElement*>(&marker);
    void* context = radiant_layout_capture_iframe_context(parent);
    EXPECT_EQ(context, parent);
    EXPECT_EQ(captured_parent, parent);
    radiant_layout_restore_iframe_context(nullptr, parent, context);
    EXPECT_EQ(restores, 1);
    EXPECT_TRUE(radiant_layout_defer_iframe_load());
    EXPECT_FLOAT_EQ(radiant_layout_geometry_fallback_dimension(nullptr, true), 23.0f);
    EXPECT_FLOAT_EQ(radiant_layout_geometry_fallback_dimension(nullptr, false), 17.0f);
    radiant_layout_iframe_load_complete(parent, frame);
    EXPECT_EQ(completions, 1);
    EXPECT_EQ(completed_frame, frame);
}
