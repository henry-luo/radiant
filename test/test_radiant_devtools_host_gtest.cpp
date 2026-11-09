#include "test_native_provider.hpp"
#include "radiant/devtools_adapter.hpp"

static UiContext* observed_ui;
static int observed_fd;
static const char* observed_path;
static RadiantDevtools* observed_inspector;
static int inspector_marker;

static RadiantDevtools* open_inspector(UiContext* ui, int fd, const char* path) {
    observed_ui = ui;
    observed_fd = fd;
    observed_path = path;
    return reinterpret_cast<RadiantDevtools*>(&inspector_marker);
}

static bool tick_inspector(RadiantDevtools* inspector) {
    observed_inspector = inspector;
    return true;
}

static void destroy_inspector(RadiantDevtools* inspector) {
    observed_inspector = inspector;
}

TEST(RadiantDevtoolsHostTest, MissingHostAndRegisteredInspector) {
    int marker = 0;
    UiContext* ui = reinterpret_cast<UiContext*>(&marker);
    RadiantDevtools* inspector = reinterpret_cast<RadiantDevtools*>(&inspector_marker);
    EXPECT_EQ(radiant_devtools_open(ui, 3, "page.html"), nullptr);
    EXPECT_FALSE(radiant_devtools_tick(inspector));
    radiant_devtools_destroy(inspector);
    static const RadiantDevtoolsHostOps incomplete = {open_inspector, nullptr, destroy_inspector};
    static const RadiantDevtoolsHostOps host = {open_inspector, tick_inspector, destroy_inspector};
    static const RadiantDevtoolsHostOps conflict = {open_inspector, tick_inspector, destroy_inspector};
    ASSERT_TRUE(expect_native_provider_registration(radiant_devtools_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_EQ(radiant_devtools_open(ui, 3, "page.html"), inspector);
    EXPECT_EQ(observed_ui, ui);
    EXPECT_EQ(observed_fd, 3);
    EXPECT_STREQ(observed_path, "page.html");
    EXPECT_FALSE(radiant_devtools_tick(nullptr));
    EXPECT_TRUE(radiant_devtools_tick(inspector));
    EXPECT_EQ(observed_inspector, inspector);
    observed_inspector = nullptr;
    radiant_devtools_destroy(inspector);
    EXPECT_EQ(observed_inspector, inspector);
}
