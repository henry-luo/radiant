#include "test_native_provider.hpp"
#include "../radiant/event_navigation_host.hpp"

static DomDocument* canceled;
static const char* destination;
static bool commit(UiContext*) { return true; }
static bool poll(UiContext*) { return false; }
static void drain(UiContext*) {}
static bool pending(UiContext*) { return true; }
static bool commit_loads(UiContext*) { return true; }
static void cancel(DomDocument* document) { canceled = document; }
static bool navigate(UiContext*, DomDocument*, const char* url) { destination = url; return true; }
static bool iframe(UiContext*, DomElement*, const char*, int, int) { return true; }

TEST(RadiantEventNavigationHostTest, ReportsIdleWithoutControllerAndRoutesRegisteredRequests) {
    int marker = 0;
    auto* ui = reinterpret_cast<UiContext*>(&marker);
    auto* document = reinterpret_cast<DomDocument*>(&marker);
    EXPECT_TRUE(radiant_event_navigation_poll_loads(ui));
    EXPECT_FALSE(radiant_event_navigation_pending_loads(ui));
    EXPECT_FALSE(radiant_event_navigation_location(ui, document, "next"));
    static const RadiantEventNavigationHostOps incomplete = {};
    static const RadiantEventNavigationHostOps host = {commit, poll, drain, pending, commit_loads, cancel, navigate, iframe};
    static const RadiantEventNavigationHostOps conflict = {commit, poll, drain, pending, commit_loads, cancel, navigate, iframe};
    ASSERT_TRUE(expect_native_provider_registration(radiant_event_navigation_host_register, &incomplete, &host, &conflict));
    EXPECT_FALSE(radiant_event_navigation_poll_loads(ui));
    EXPECT_TRUE(radiant_event_navigation_pending_loads(ui));
    EXPECT_TRUE(radiant_event_navigation_location(ui, document, "next"));
    EXPECT_STREQ(destination, "next");
    radiant_event_navigation_cancel_loads(document);
    EXPECT_EQ(canceled, document);
}
