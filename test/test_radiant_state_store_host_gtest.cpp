#include "test_native_provider.hpp"
#include "../radiant/state_store_host.hpp"

static EvalContext* provided_owner;
static EvalContext* observed_owner;
static DocState* observed_state;
static uint64_t* observed_root;
static bool root_registration_succeeds = true;

static bool initialize(DomDocument*, DocState* state, EvalContext** owner) {
    observed_state = state;
    *owner = provided_owner;
    return true;
}
static bool enter(EvalContext* owner) { observed_owner = owner; return true; }
static EvalContext* current() { return provided_owner; }
static void release_root(EvalContext* owner, uint64_t* root) {
    observed_owner = owner;
    observed_root = root;
}
static bool register_current_root(uint64_t* root) {
    observed_root = root;
    return root_registration_succeeds;
}
static void release_current_root(uint64_t* root) { observed_root = root; }
static void detach_maps(DocState* state) { observed_state = state; }

TEST(RadiantStateStoreHostTest, MissingHostAndRegisteredOperations) {
    int marker = 0;
    auto* owner = reinterpret_cast<EvalContext*>(&marker);
    auto* state = reinterpret_cast<DocState*>(&marker);
    EvalContext* actual = owner;
    EXPECT_TRUE(radiant_state_store_host_initialize(nullptr, state, &actual));
    EXPECT_EQ(actual, nullptr);
    EXPECT_TRUE(radiant_state_store_host_enter(nullptr));
    EXPECT_FALSE(radiant_state_store_host_enter(owner));
    EXPECT_EQ(radiant_state_store_host_current(), nullptr);
    uint64_t root = 0;
    EXPECT_FALSE(radiant_state_store_host_register_current_root(&root));
    EXPECT_DEATH_IF_SUPPORTED(radiant_state_store_host_release_root(owner, &root), "");
    EXPECT_DEATH_IF_SUPPORTED(radiant_state_store_host_release_current_root(&root), "");
    EXPECT_DEATH_IF_SUPPORTED(radiant_state_store_host_detach_maps(state), "");
    static const RadiantStateStoreHostOps incomplete = {initialize, enter, current, release_root, register_current_root, nullptr, detach_maps};
    static const RadiantStateStoreHostOps host = {initialize, enter, current, release_root, register_current_root, release_current_root, detach_maps};
    static const RadiantStateStoreHostOps conflict = {initialize, enter, current, release_root, register_current_root, release_current_root, detach_maps};
    ASSERT_TRUE(expect_native_provider_registration(radiant_state_store_host_register,
                                                     &incomplete, &host, &conflict));
    provided_owner = owner;
    EXPECT_TRUE(radiant_state_store_host_initialize(nullptr, state, &actual));
    EXPECT_EQ(actual, owner);
    EXPECT_EQ(observed_state, state);
    EXPECT_TRUE(radiant_state_store_host_enter(owner));
    EXPECT_EQ(observed_owner, owner);
    EXPECT_EQ(radiant_state_store_host_current(), owner);
    radiant_state_store_host_release_root(owner, &root);
    EXPECT_EQ(observed_root, &root);
    observed_root = nullptr;
    EXPECT_TRUE(radiant_state_store_host_register_current_root(&root));
    EXPECT_EQ(observed_root, &root);
    root_registration_succeeds = false;
    EXPECT_FALSE(radiant_state_store_host_register_current_root(&root));
    observed_root = nullptr;
    radiant_state_store_host_release_current_root(&root);
    EXPECT_EQ(observed_root, &root);
    observed_state = nullptr;
    radiant_state_store_host_detach_maps(state);
    EXPECT_EQ(observed_state, state);
}
