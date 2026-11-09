#include "test_native_provider.hpp"
#include "radiant/editing_dom_host.hpp"
#include "radiant/script_loop_host.hpp"

static DomDocument* observed_document;
static DomElement* observed_parent;
static DomNode* observed_child;
static DomNode* observed_reference;
static const char* observed_text;
static DomJsMutationKind observed_kind;
static const char* observed_old_value;
static bool available = true;

static bool host_available() { return available; }

static bool insert_child(DomElement* parent, DomNode* child, DomNode* reference) {
    observed_parent = parent;
    observed_child = child;
    observed_reference = reference;
    return reference != nullptr;
}
static bool remove_child(DomNode* child) { observed_child = child; return true; }
static bool insert_html(DomDocument* document, const char* html) {
    observed_document = document;
    observed_text = html;
    return true;
}
static void notify(DomJsMutationKind kind, DomNode* target, DomNode* parent,
                   const char* attribute_name, const char* old_value) {
    observed_kind = kind;
    observed_child = target;
    observed_reference = parent;
    observed_text = attribute_name;
    observed_old_value = old_value;
}

TEST(RadiantEditingDomHostTest, MissingHostAndRegisteredOperations) {
    int marker = 0;
    auto* document = reinterpret_cast<DomDocument*>(&marker);
    auto* parent = reinterpret_cast<DomElement*>(&marker);
    auto* child = reinterpret_cast<DomNode*>(&marker);
    bool host_present = true;
    EXPECT_FALSE(radiant_editing_dom_insert_child(parent, child, nullptr, &host_present));
    EXPECT_FALSE(host_present);
    EXPECT_FALSE(radiant_editing_dom_remove_child(child, &host_present));
    EXPECT_FALSE(radiant_editing_dom_insert_html(document, "x"));
    EXPECT_FALSE(radiant_editing_dom_notify_host(DOM_JS_MUTATION_TEXT, child, nullptr, nullptr, nullptr));
    static const RadiantEditingDomHostOps incomplete = {host_available, insert_child, remove_child, insert_html, nullptr};
    static const RadiantEditingDomHostOps host = {host_available, insert_child, remove_child, insert_html, notify};
    static const RadiantEditingDomHostOps conflict = {host_available, insert_child, remove_child, insert_html, notify};
    ASSERT_TRUE(expect_native_provider_registration(radiant_editing_dom_host_register,
                                                     &incomplete, &host, &conflict));
    available = false;
    EXPECT_FALSE(radiant_editing_dom_insert_child(parent, child, nullptr, &host_present));
    EXPECT_FALSE(host_present);
    EXPECT_FALSE(radiant_editing_dom_remove_child(child, &host_present));
    EXPECT_FALSE(radiant_editing_dom_insert_html(document, "x"));
    EXPECT_FALSE(radiant_editing_dom_notify_host(DOM_JS_MUTATION_TEXT, child, nullptr, nullptr, nullptr));
    available = true;
    EXPECT_FALSE(radiant_editing_dom_insert_child(parent, child, nullptr, &host_present));
    EXPECT_TRUE(host_present);
    EXPECT_TRUE(radiant_editing_dom_insert_child(parent, child, child, &host_present));
    EXPECT_EQ(observed_parent, parent);
    EXPECT_EQ(observed_child, child);
    EXPECT_EQ(observed_reference, child);
    observed_child = nullptr;
    EXPECT_TRUE(radiant_editing_dom_remove_child(child, &host_present));
    EXPECT_EQ(observed_child, child);
    EXPECT_TRUE(radiant_editing_dom_insert_html(document, "html"));
    EXPECT_EQ(observed_document, document);
    EXPECT_STREQ(observed_text, "html");
    EXPECT_TRUE(radiant_editing_dom_notify_host(DOM_JS_MUTATION_ATTRIBUTE, child, child, "title", "old"));
    EXPECT_EQ(observed_kind, DOM_JS_MUTATION_ATTRIBUTE);
    EXPECT_EQ(observed_child, child);
    EXPECT_EQ(observed_reference, child);
    EXPECT_STREQ(observed_text, "title");
    EXPECT_STREQ(observed_old_value, "old");
}

static bool loop_begin(UiContext*, DomDocument*, RadiantScriptLoopTurn* turn) {
    turn->active = true;
    turn->saved_input = &observed_document;
    return true;
}
static bool loop_pump(bool active, bool advance, int wait_ms, double delta_ms, int steps) {
    return active && advance && wait_ms == 4 && delta_ms == 5.0 && steps == 6;
}
static void loop_end(RadiantScriptLoopTurn* turn) { turn->saved_input = nullptr; }
static bool loop_bind(DomDocument* doc) { return doc == observed_document; }
static void loop_cancel(DomDocument* doc) { observed_document = doc; }
static bool loop_quiescent() { return available; }
static void loop_detach() { observed_text = "detach"; }
static void loop_cleanup(bool reset_dom_cache) { observed_text = reset_dom_cache ? "cleanup-dom" : "cleanup"; }
static void loop_reset() { observed_text = "reset"; }
static bool loop_frame(double time, bool* ran) {
    if (ran) *ran = time == 5.0;
    return time == 5.0;
}
static void loop_configure(bool retain, bool, bool) { available = retain; }
static uint64_t loop_progress() { return 42; }
static bool loop_edit_transform(bool source, const LambdaDocumentTransformConfig**,
                                const LambdaDocumentTransformOption**, int* count) {
    if (count) *count = source ? 1 : 0;
    return true;
}

TEST(RadiantScriptLoopHostTest, MissingHostAndRegisteredOperations) {
    RadiantScriptLoopTurn turn = {};
    ASSERT_TRUE(radiant_script_loop_begin(nullptr, nullptr, &turn));
    EXPECT_FALSE(turn.active);
    EXPECT_FALSE(radiant_script_loop_pump(&turn, true, 4, 5.0, 6));
    radiant_script_loop_end(&turn);
    EXPECT_TRUE(radiant_script_window_bind_document(nullptr));
    EXPECT_TRUE(radiant_script_window_quiescent());
    bool ran = true;
    EXPECT_FALSE(radiant_script_window_animation_frame(5.0, &ran));
    EXPECT_FALSE(ran);
    static const RadiantScriptLoopHostOps incomplete = {loop_begin, loop_pump, nullptr};
    static const RadiantScriptLoopHostOps host = {loop_begin, loop_pump, loop_end,
        loop_bind, loop_cancel, loop_quiescent, loop_detach, loop_cleanup, loop_reset,
        loop_frame, loop_configure, loop_progress, loop_edit_transform};
    static const RadiantScriptLoopHostOps conflict = {loop_begin, loop_pump, loop_end,
        loop_bind, loop_cancel, loop_quiescent, loop_detach, loop_cleanup, loop_reset,
        loop_frame, loop_configure, loop_progress, loop_edit_transform};
    ASSERT_TRUE(expect_native_provider_registration(radiant_script_loop_host_register,
                                                     &incomplete, &host, &conflict));
    ASSERT_TRUE(radiant_script_loop_begin(nullptr, nullptr, &turn));
    EXPECT_TRUE(turn.active);
    EXPECT_EQ(turn.saved_input, &observed_document);
    EXPECT_TRUE(radiant_script_loop_pump(&turn, true, 4, 5.0, 6));
    radiant_script_loop_end(&turn);
    EXPECT_EQ(turn.saved_input, nullptr);
    int marker = 0;
    auto* document = reinterpret_cast<DomDocument*>(&marker);
    radiant_script_window_cancel_document(document);
    EXPECT_TRUE(radiant_script_window_bind_document(document));
    radiant_script_window_configure(false, false, true);
    EXPECT_FALSE(radiant_script_window_quiescent());
    radiant_script_window_configure(true, false, true);
    EXPECT_TRUE(radiant_script_window_quiescent());
    EXPECT_TRUE(radiant_script_window_animation_frame(5.0, &ran));
    EXPECT_TRUE(ran);
    radiant_script_window_detach();
    EXPECT_STREQ(observed_text, "detach");
    radiant_script_window_cleanup_batch(false);
    EXPECT_STREQ(observed_text, "cleanup");
    radiant_script_window_cleanup_batch(true);
    EXPECT_STREQ(observed_text, "cleanup-dom");
    radiant_script_window_reset_clock();
    EXPECT_STREQ(observed_text, "reset");
    EXPECT_EQ(radiant_script_window_progress_generation(), 42);
    int option_count = 0;
    EXPECT_TRUE(radiant_script_window_edit_transform(true, nullptr, nullptr, &option_count));
    EXPECT_EQ(option_count, 1);
}
