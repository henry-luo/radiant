#include "test_native_provider.hpp"
#include "radiant/event_reconcile_host.hpp"

static DomNode* observed_old;
static DomNode* observed_new;
static bool prepare(DomDocument*, const char**) { return true; }
static bool templates(UiContext*, DomDocument*) { return true; }
static void mutations(UiContext*, DomDocument*) {}
static void attribute(DomElement*, const char*, const char*) {}
static void render_events(DomDocument*, DomNode*) {}
static void follow(DomDocument*, DomNode* old_root, DomNode* new_root) { observed_old = old_root; observed_new = new_root; }

TEST(RadiantEventReconcileHostTest, ReportsUnavailableThenRoutesRebuild) {
    const char* reason = nullptr;
    EXPECT_FALSE(radiant_event_reconcile_prepare_layout(nullptr, &reason));
    EXPECT_STREQ(reason, "event-controller-unavailable");
    static const RadiantEventReconcileHostOps incomplete = {};
    static const RadiantEventReconcileHostOps host = {prepare, prepare, templates, mutations, attribute, render_events, follow};
    static const RadiantEventReconcileHostOps conflict = {prepare, prepare, templates, mutations, attribute, render_events, follow};
    ASSERT_TRUE(expect_native_provider_registration(radiant_event_reconcile_host_register, &incomplete, &host, &conflict));
    int first = 0, second = 0;
    auto* old_root = reinterpret_cast<DomNode*>(&first);
    auto* new_root = reinterpret_cast<DomNode*>(&second);
    EXPECT_TRUE(radiant_event_reconcile_prepare_layout(nullptr, nullptr));
    radiant_event_reconcile_follow_rebuild(nullptr, old_root, new_root);
    EXPECT_EQ(observed_old, old_root);
    EXPECT_EQ(observed_new, new_root);
}
