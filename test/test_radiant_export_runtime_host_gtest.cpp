#include "test_native_provider.hpp"
#include "radiant/export_runtime_host.hpp"

static int owner_marker;
static int releases;
static bool blocked;
static bool acquire(void** owner) { *owner = &owner_marker; return true; }
static bool release(void* owner) { if (owner != &owner_marker) return false; releases++; return true; }
static bool retirement_blocked() { return blocked; }
static DomDocument* load_transform(RenderExportSession*, int, int, const char*,
    const LambdaDocumentTransformConfig*, const LambdaDocumentTransformOption*, int, InputResourcePolicy) {
    return reinterpret_cast<DomDocument*>(&owner_marker);
}

TEST(RadiantExportRuntimeHostTest, MissingHostAndBorrowLifecycle) {
    void* owner = &owner_marker;
    EXPECT_FALSE(radiant_export_runtime_acquire(nullptr));
    EXPECT_TRUE(radiant_export_runtime_acquire(&owner));
    EXPECT_EQ(owner, nullptr);
    EXPECT_TRUE(radiant_export_runtime_release(nullptr));
    EXPECT_FALSE(radiant_export_runtime_release(&owner_marker));
    EXPECT_FALSE(radiant_export_runtime_retirement_blocked());
    EXPECT_EQ(radiant_export_runtime_load_transform(nullptr, 0, 0, nullptr, nullptr, nullptr, 0,
        INPUT_RESOURCE_LOCAL_ONLY), nullptr);
    static const RadiantExportRuntimeHostOps incomplete = {acquire, release, retirement_blocked, nullptr};
    static const RadiantExportRuntimeHostOps host = {acquire, release, retirement_blocked, load_transform};
    static const RadiantExportRuntimeHostOps conflict = {acquire, release, retirement_blocked, load_transform};
    ASSERT_TRUE(expect_native_provider_registration(radiant_export_runtime_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_export_runtime_acquire(&owner));
    EXPECT_EQ(owner, &owner_marker);
    EXPECT_TRUE(radiant_export_runtime_release(owner));
    EXPECT_EQ(releases, 1);
    EXPECT_EQ(radiant_export_runtime_load_transform(nullptr, 0, 0, nullptr, nullptr, nullptr, 0,
        INPUT_RESOURCE_LOCAL_ONLY), reinterpret_cast<DomDocument*>(&owner_marker));
    blocked = true;
    EXPECT_TRUE(radiant_export_runtime_retirement_blocked());
    blocked = false;
    EXPECT_FALSE(radiant_export_runtime_retirement_blocked());
}
