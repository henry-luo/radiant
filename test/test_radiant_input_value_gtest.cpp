#include <gtest/gtest.h>
#include <string.h>
#include "radiant/input_value.hpp"
#include "lib/document_lifetime_guard.h"
#include "radiant/select_options.hpp"
#include "radiant/form_controls.hpp"
#include "radiant/loader_runtime_host.hpp"
#include "radiant/reactive_render_host.hpp"
#include "radiant/stylesheet_sync.hpp"
#include "radiant/resource_loaders.h"
#include "lambda/network/network_resource_manager.h"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/css_engine.hpp"
#include "lambda/input/input.hpp"
#include "test_native_input_document.hpp"
#include "test_native_provider.hpp"

static UiContext* loader_test_host;
static DomDocument* loader_test_document;
static Runtime* loader_test_runtime;

TEST(RadiantInputValueTest, NativeDocumentLifetimeGuardProtectsItsOwner) {
    int document = 0;
    int other = 0;
    EXPECT_FALSE(lambda_document_lifetime_guard_active(&document));
    LambdaDocumentLifetimeGuard* guard = lambda_document_lifetime_guard_begin(&document);
    ASSERT_NE(guard, nullptr);
    EXPECT_TRUE(lambda_document_lifetime_guard_active(&document));
    EXPECT_TRUE(lambda_document_lifetime_guard_owns(guard, &document));
    EXPECT_FALSE(lambda_document_lifetime_guard_owns(guard, &other));
    EXPECT_EQ(lambda_document_lifetime_guard_begin(&document), nullptr);
    EXPECT_TRUE(lambda_document_lifetime_guard_end(guard));
    EXPECT_FALSE(lambda_document_lifetime_guard_active(&document));
    EXPECT_FALSE(lambda_document_lifetime_guard_is_current(guard));
}

TEST(RadiantInputValueTest, NativeInlineStylesheetsTrackTextAndDocumentOrder) {
    NativeInputDocument fixture;
    DomDocument* document = fixture.document;
    ASSERT_NE(document, nullptr);
    ASSERT_TRUE(css_property_system_init(document->document_pool));
    DomElement* root = dom_create_backed_element_identity(document, "div", nullptr, false);
    DomElement* first = dom_create_backed_element_identity(document, "style", nullptr, false);
    DomElement* second = dom_create_backed_element_identity(document, "style", nullptr, false);
    ASSERT_TRUE(root && first && second);
    document->root = lam::up(root);
    ASSERT_TRUE(root->append_child(first));
    ASSERT_TRUE(root->append_child(second));
    const char* first_css = ".a { color: red; }";
    const char* second_css = ".b { color: blue; }";
    DomText* first_text = DomText::create_detached_copy(document, first_css, strlen(first_css));
    DomText* second_text = DomText::create_detached_copy(document, second_css, strlen(second_css));
    ASSERT_TRUE(first_text && second_text);
    ASSERT_TRUE(first->append_child(first_text));
    ASSERT_TRUE(second->append_child(second_text));
    ASSERT_TRUE(radiant_sync_inline_stylesheet(second));
    ASSERT_TRUE(radiant_sync_inline_stylesheet(first));
    ASSERT_EQ(document->stylesheet_count, 2);
    EXPECT_EQ(document->stylesheets[0]->owner_element, first);
    EXPECT_EQ(document->stylesheets[1]->owner_element, second);
    EXPECT_EQ(document->stylesheets[0]->rule_count, 1u);
    CssStylesheet* old = document->stylesheets[0];
    ASSERT_TRUE(first->remove_child(first_text));
    const char* replacement = ".a { color: green; } .c { width: 1px; }";
    DomText* replacement_text = DomText::create_detached_copy(document, replacement, strlen(replacement));
    ASSERT_NE(replacement_text, nullptr);
    ASSERT_TRUE(first->append_child(replacement_text));
    ASSERT_TRUE(radiant_sync_inline_stylesheet(first));
    EXPECT_NE(document->stylesheets[0], old);
    EXPECT_EQ(document->stylesheets[0]->rule_count, 2u);
    ASSERT_TRUE(root->remove_child(first));
    ASSERT_TRUE(radiant_sync_inline_stylesheet(first));
    ASSERT_EQ(document->stylesheet_count, 1);
    EXPECT_EQ(document->stylesheets[0]->owner_element, second);
}
TEST(RadiantInputValueTest, DownloadedStylesheetsUseOwnerOrderAcrossCompletionOrder) {
    NativeInputDocument fixture;
    DomDocument* document = fixture.document;
    ASSERT_NE(document, nullptr);
    ASSERT_TRUE(css_property_system_init(document->document_pool));
    DomElement* root = dom_create_backed_element_identity(document, "head", nullptr, false);
    DomElement* first = dom_create_backed_element_identity(document, "link", nullptr, false);
    DomElement* middle = dom_create_backed_element_identity(document, "style", nullptr, false);
    DomElement* last = dom_create_backed_element_identity(document, "link", nullptr, false);
    ASSERT_TRUE(root && first && middle && last);
    document->root = lam::up(root);
    ASSERT_TRUE(root->append_child(first));
    ASSERT_TRUE(root->append_child(middle));
    ASSERT_TRUE(root->append_child(last));
    const char* css = ".sample { color: red; }";
    DomText* text = DomText::create_detached_copy(document, css, strlen(css));
    ASSERT_NE(text, nullptr);
    ASSERT_TRUE(middle->append_child(text));
    ASSERT_TRUE(radiant_sync_inline_stylesheet(middle));
    NetworkResource resource = {};
    char url[] = "https://example.test/order.css";
    char path[] = "test/fixtures/stylesheet-order.css";
    resource.url = url;
    resource.local_path = path;
    resource.state = STATE_COMPLETED;
    resource.owner_element = last;
    process_css_resource(&resource, document);
    resource.state = STATE_CACHED;
    resource.owner_element = first;
    process_css_resource(&resource, document);
    ASSERT_EQ(document->stylesheet_count, 3);
    EXPECT_EQ(document->stylesheets[0]->owner_element, first);
    EXPECT_EQ(document->stylesheets[1]->owner_element, middle);
    EXPECT_EQ(document->stylesheets[2]->owner_element, last);
    EXPECT_EQ(document->stylesheets[0]->rule_count, 1u);
    EXPECT_EQ(document->stylesheets[2]->rule_count, 1u);
}

TEST(RadiantInputValueTest, DownloadedStylesheetsRespectLinkMediaAndDisabledState) {
    NativeInputDocument fixture;
    DomDocument* document = fixture.document;
    ASSERT_NE(document, nullptr);
    ASSERT_TRUE(css_property_system_init(document->document_pool));
    DomElement* root = dom_create_backed_element_identity(document, "head", nullptr, false);
    ASSERT_NE(root, nullptr);
    document->root = lam::up(root);
    CssEngine* engine = css_engine_create(document->document_pool);
    ASSERT_NE(engine, nullptr);
    const char* queries[] = {"(prefers-color-scheme: dark)", "(prefers-color-scheme: light)", "all"};
    for (int i = 0; i < 3; i++) {
        SCOPED_TRACE(queries[i]);
        DomElement* link = dom_create_backed_element_identity(document, "link", nullptr, false);
        ASSERT_NE(link, nullptr);
        ASSERT_TRUE(link->set_attribute("rel", "stylesheet"));
        ASSERT_TRUE(link->set_attribute("media", queries[i]));
        if (i == 2) ASSERT_TRUE(link->set_attribute("disabled", ""));
        ASSERT_TRUE(root->append_child(link));
        NetworkResource resource = {};
        char url[] = "https://example.test/theme.css";
        char local_path[] = "test/fixtures/stylesheet-order.css";
        resource.url = url;
        resource.local_path = local_path;
        resource.state = i % 2 ? STATE_CACHED : STATE_COMPLETED;
        resource.owner_element = link;
        process_css_resource(&resource, document);
        ASSERT_EQ(document->stylesheet_count, i + 1);
        CssStylesheet* sheet = document->stylesheets[i];
        EXPECT_STREQ(sheet->media, queries[i]);
        EXPECT_EQ(css_evaluate_media_query(engine, queries[i]), i != 0);
        int active_rules = 0;
        ASSERT_TRUE(css_stylesheet_visit_active_rules(engine, sheet,
            [](void* context, const CssRule*) {
                (*(int*)context)++;
                return true;
            }, &active_rules));
        EXPECT_EQ(active_rules, i == 1 ? 1 : 0);
    }
    css_engine_destroy(engine);
}

static Runtime* loader_test_get(UiContext* host) { loader_test_host = host; return loader_test_runtime; }
static void loader_test_finish(DomDocument* doc, Runtime* runtime) {
    loader_test_document = doc;
    loader_test_runtime = runtime;
}
static void loader_test_release(Runtime* runtime) { loader_test_runtime = runtime; }
static bool loader_test_begin(UiContext*, Url*, const char*, const LambdaDocumentTransformConfig*,
                              const LambdaDocumentTransformOption*, int, InputResourcePolicy,
                              RadiantScriptDocumentLoad* load) {
    load->runtime = loader_test_runtime;
    return true;
}
static void loader_test_cancel(RadiantScriptDocumentLoad* load, bool) { load->runtime = nullptr; }
static void loader_test_complete(RadiantScriptDocumentLoad* load, DomDocument* doc, Element*) {
    loader_test_document = doc;
    load->runtime = nullptr;
}
static bool loader_test_prepare() { loader_test_document = nullptr; return true; }
static bool loader_test_embedded(UiContext* host, Input*, Runtime** owned, Runtime** shared) {
    loader_test_host = host;
    *owned = loader_test_runtime;
    *shared = nullptr;
    return true;
}
static void loader_test_html_scripts(Element*, DomDocument* doc, Pool*, Url*, DocumentScriptPhaseTiming*) {
    loader_test_document = doc;
}
static void loader_test_html_handlers(DomDocument* doc) { loader_test_document = doc; }
static void loader_test_timing_begin() { loader_test_host = nullptr; }
static void loader_test_timing_end(RadiantScriptPhaseSummary* timing) { timing->parse_us = 37; }
static void loader_test_script_cleanup(DomDocument* doc) { loader_test_document = doc; }
static Item reactive_test_root() { return Item{.item = i2it(7)}; }
static Item reactive_test_result(Item source, const char*) { return source; }
static bool reactive_test_equal(Item left, Item right) { return left.item == right.item; }
static bool reactive_test_metadata(DomElement*, const char* name, const char* value) {
    return name && value && strcmp(name, "data-a") == 0 && strcmp(value, "value") == 0;
}

TEST(RadiantInputValueTest, ReactiveRenderHostIsOptionalAndChecksRegistration) {
    Item value = {.item = i2it(7)};
    EXPECT_FALSE(radiant_reactive_render_host_available());
    EXPECT_EQ(radiant_reactive_render_document_root().item, ItemNull.item);
    EXPECT_EQ(radiant_reactive_render_result(value, "t").item, ItemNull.item);
    EXPECT_FALSE(radiant_reactive_render_equal(value, value));
    static const RadiantReactiveRenderHostOps incomplete = {
        reactive_test_root, reactive_test_result, nullptr, reactive_test_metadata};
    static const RadiantReactiveRenderHostOps operations = {
        reactive_test_root, reactive_test_result, reactive_test_equal, reactive_test_metadata};
    static const RadiantReactiveRenderHostOps conflict = {
        reactive_test_root, reactive_test_result, reactive_test_equal, reactive_test_metadata};
    ASSERT_TRUE(expect_native_provider_registration(radiant_reactive_render_host_register,
                                                     &incomplete, &operations, &conflict));
    EXPECT_TRUE(radiant_reactive_render_host_available());
    EXPECT_EQ(radiant_reactive_render_document_root().item, value.item);
    EXPECT_EQ(radiant_reactive_render_result(value, "t").item, value.item);
    EXPECT_TRUE(radiant_reactive_render_equal(value, value));
    EXPECT_TRUE(radiant_reactive_render_set_metadata(nullptr, "data-a", "value"));
}
static const LambdaDocumentTransformConfig* loader_test_transform(const char*) {
    return reinterpret_cast<const LambdaDocumentTransformConfig*>(loader_test_runtime);
}

static bool loader_test_cleanup_owners(DomDocument* document, RadiantDocumentCleanupOwners* owners) {
    loader_test_document = document;
    owners->timer = reinterpret_cast<EvalContext*>(loader_test_runtime);
    return true;
}

static Runtime* loader_test_defer_script_cleanup(DomDocument* document) {
    loader_test_document = document;
    return loader_test_runtime;
}

static int loader_test_retirement_calls;
static void loader_test_release_observers(DomDocument*) { loader_test_retirement_calls |= 1; }
static void loader_test_release_dom(DomDocument*) { loader_test_retirement_calls |= 2; }
static void loader_test_cancel_timers(DomDocument*, EvalContext*) { loader_test_retirement_calls |= 4; }
static bool loader_test_prepare_cleanup(EvalContext*, EvalContext*) { loader_test_retirement_calls |= 8; return true; }
static bool loader_test_bind_owner(EvalContext*) { loader_test_retirement_calls |= 16; return true; }
static bool loader_test_switch_context(EvalContext*) { loader_test_retirement_calls |= 32; return true; }
static bool loader_test_release_navigation() { loader_test_retirement_calls |= 256; return true; }
static bool loader_test_finish_navigation() { loader_test_retirement_calls |= 512; return true; }
static bool loader_test_bind_document(UiContext*, DomDocument*) { loader_test_retirement_calls |= 1024; return true; }
static Item loader_test_emit(Item, Item) { return ItemNull; }
static bool loader_test_register_event_hooks(RadiantEventEmitFn emit) {
    loader_test_retirement_calls |= 4096;
    return emit != nullptr;
}
static bool loader_test_bind_event(UiContext*, DomDocument*, bool allow_transfer, bool rebind_same) {
    loader_test_retirement_calls |= 2048;
    return allow_transfer && !rebind_same;
}

static EvalContext* loader_test_runtime_owner(Runtime* runtime) {
    return reinterpret_cast<EvalContext*>(runtime);
}
static bool loader_test_runtime_is_cleaning(Runtime*) { return false; }
static void loader_test_detach_document(Runtime*, DomDocument*) { loader_test_retirement_calls |= 64; }
static void loader_test_cleanup_runtime(Runtime*) { loader_test_retirement_calls |= 128; }
static RadiantRuntimeCleanupResult loader_test_try_cleanup(Runtime*, bool wait, bool pinned) {
    return wait && pinned ? RADIANT_RUNTIME_CLEANUP_COMPLETE : RADIANT_RUNTIME_CLEANUP_RETRY;
}

static int loader_test_release_tasks(DomDocument*) { return 0; }
static bool loader_test_pending_work(EvalContext*, DomDocument*) { return true; }
static bool loader_test_pin_runtime(Runtime*) { return true; }
static bool loader_test_unpin_runtime(Runtime*) { return true; }
static bool loader_test_cancel_tasks(EvalContext*, DomDocument*) { return true; }
static RadiantDocumentJoinProgress loader_test_progress_tasks(EvalContext*, DomDocument*, LambdaDocumentLifetimeGuard*) {
    return RADIANT_DOCUMENT_JOIN_ADVANCED;
}

TEST(RadiantInputValueTest, LoaderRuntimeHostIsOptionalAndChecksRegistration) {
    int marker = 0;
    auto* host = reinterpret_cast<UiContext*>(&marker);
    auto* document = reinterpret_cast<DomDocument*>(&marker);
    auto* runtime = reinterpret_cast<Runtime*>(&marker);
    EXPECT_EQ(ui_context_loader_runtime(host), nullptr);
    EXPECT_EQ(radiant_loader_transform_for_type("math"), nullptr);
    EXPECT_FALSE(radiant_loader_prepare_script());
    EXPECT_FALSE(radiant_loader_bind_document(host, document));
    EXPECT_FALSE(radiant_loader_register_event_hooks(loader_test_emit));
    RadiantScriptDocumentLoad load = {};
    EXPECT_FALSE(radiant_loader_begin_script(host, nullptr, "source", nullptr, nullptr, 0,
                                             INPUT_RESOURCE_LOCAL_ONLY, &load));
    Runtime* owned = nullptr;
    Runtime* shared = nullptr;
    EXPECT_FALSE(radiant_loader_render_embedded(host, nullptr, &owned, &shared));
    RadiantScriptPhaseSummary timing = {};
    timing.parse_us = 12;
    radiant_loader_end_document_timing(&timing);
    EXPECT_EQ(timing.parse_us, 0);
    NativeInputDocument native;
    ASSERT_NE(native.document, nullptr);
    EXPECT_TRUE(radiant_loader_cleanup_html_script(native.document));
    RadiantDocumentCleanupOwners owners = {};
    EXPECT_TRUE(radiant_loader_cleanup_owners(native.document, &owners));
    EXPECT_EQ(owners.timer, nullptr);
    EXPECT_TRUE(radiant_loader_release_navigation_evaluator(native.document));
    EXPECT_TRUE(radiant_loader_prepare_cleanup(nullptr, nullptr));
    EXPECT_FALSE(radiant_loader_bind_owner(reinterpret_cast<EvalContext*>(runtime)));
    EXPECT_FALSE(radiant_loader_switch_context(reinterpret_cast<EvalContext*>(runtime)));
    EXPECT_FALSE(radiant_loader_prepare_cleanup(reinterpret_cast<EvalContext*>(runtime), nullptr));
    native.document->js.runtime = runtime;
    EXPECT_FALSE(radiant_loader_cleanup_html_script(native.document));
    EXPECT_FALSE(radiant_loader_cleanup_owners(native.document, &owners));
    EXPECT_FALSE(radiant_loader_release_navigation_evaluator(native.document));
    native.document->js.runtime = nullptr;
    native.document->js.preamble_state = &marker;
    EXPECT_FALSE(radiant_loader_cleanup_owners(native.document, &owners));
    native.document->js.preamble_state = nullptr;
    static const RadiantLoaderRuntimeHostOps incomplete = {loader_test_get, nullptr, loader_test_release};
    static const RadiantLoaderRuntimeHostOps operations = {loader_test_get, loader_test_finish,
        loader_test_release, loader_test_begin, loader_test_cancel, loader_test_complete,
        loader_test_prepare, loader_test_embedded, loader_test_transform,
        loader_test_html_scripts, loader_test_html_handlers,
        loader_test_timing_begin, loader_test_timing_end, loader_test_script_cleanup, loader_test_defer_script_cleanup,
        loader_test_cleanup_owners,
        loader_test_release_observers, loader_test_release_dom, loader_test_cancel_timers,
        loader_test_prepare_cleanup, loader_test_bind_owner, loader_test_switch_context,
        loader_test_release_navigation, loader_test_finish_navigation, loader_test_bind_document,
        loader_test_bind_event, loader_test_register_event_hooks,
        loader_test_runtime_owner, loader_test_runtime_is_cleaning, loader_test_detach_document,
        loader_test_cleanup_runtime, loader_test_try_cleanup,
        loader_test_release_tasks, loader_test_pending_work, loader_test_pin_runtime,
        loader_test_unpin_runtime, loader_test_cancel_tasks, loader_test_progress_tasks};
    static const RadiantLoaderRuntimeHostOps conflict = {loader_test_get, loader_test_finish,
        loader_test_release, loader_test_begin, loader_test_cancel, loader_test_complete,
        loader_test_prepare, loader_test_embedded, loader_test_transform,
        loader_test_html_scripts, loader_test_html_handlers,
        loader_test_timing_begin, loader_test_timing_end, loader_test_script_cleanup, loader_test_defer_script_cleanup,
        loader_test_cleanup_owners,
        loader_test_release_observers, loader_test_release_dom, loader_test_cancel_timers,
        loader_test_prepare_cleanup, loader_test_bind_owner, loader_test_switch_context,
        loader_test_release_navigation, loader_test_finish_navigation, loader_test_bind_document,
        loader_test_bind_event, loader_test_register_event_hooks,
        loader_test_runtime_owner, loader_test_runtime_is_cleaning, loader_test_detach_document,
        loader_test_cleanup_runtime, loader_test_try_cleanup,
        loader_test_release_tasks, loader_test_pending_work, loader_test_pin_runtime,
        loader_test_unpin_runtime, loader_test_cancel_tasks, loader_test_progress_tasks};
    ASSERT_TRUE(expect_native_provider_registration(radiant_loader_runtime_host_register,
                                                     &incomplete, &operations, &conflict));
    loader_test_runtime = runtime;
    EXPECT_TRUE(radiant_loader_cleanup_owners(document, &owners));
    EXPECT_EQ(owners.timer, reinterpret_cast<EvalContext*>(runtime));
    EXPECT_EQ(loader_test_document, document);
    EXPECT_EQ(radiant_loader_defer_html_script_cleanup(document), runtime);
    loader_test_retirement_calls = 0;
    radiant_loader_release_observers(document);
    radiant_loader_release_dom(document);
    radiant_loader_cancel_document_timers(document, owners.timer);
    EXPECT_TRUE(radiant_loader_prepare_cleanup(owners.timer, nullptr));
    EXPECT_TRUE(radiant_loader_bind_owner(owners.timer));
    EXPECT_TRUE(radiant_loader_switch_context(owners.timer));
    EXPECT_EQ(radiant_loader_runtime_owner(runtime), owners.timer);
    EXPECT_FALSE(radiant_loader_runtime_is_cleaning(runtime));
    radiant_loader_detach_document(runtime, document);
    EXPECT_TRUE(radiant_loader_cleanup_runtime(runtime));
    EXPECT_EQ(radiant_loader_try_cleanup_runtime(runtime, true, true), RADIANT_RUNTIME_CLEANUP_COMPLETE);
    EXPECT_EQ(radiant_loader_try_cleanup_runtime(runtime, false, true), RADIANT_RUNTIME_CLEANUP_RETRY);
    EXPECT_EQ(radiant_loader_release_document_tasks(document), 0);
    EXPECT_TRUE(radiant_loader_pending_native_work(owners.timer, document));
    EXPECT_TRUE(radiant_loader_pin_runtime(runtime));
    EXPECT_TRUE(radiant_loader_unpin_runtime(runtime));
    EXPECT_TRUE(radiant_loader_cancel_document_tasks(owners.timer, document));
    EXPECT_EQ(radiant_loader_progress_document_tasks(owners.timer, document, nullptr), RADIANT_DOCUMENT_JOIN_ADVANCED);
    EXPECT_TRUE(radiant_loader_release_navigation_evaluator(document));
    EXPECT_TRUE(radiant_loader_finish_navigation_worker(document));
    EXPECT_TRUE(radiant_loader_bind_document(host, document));
    EXPECT_TRUE(radiant_loader_bind_event_document(host, document, true, false));
    EXPECT_FALSE(radiant_loader_bind_event_document(host, document, false, true));
    EXPECT_TRUE(radiant_loader_register_event_hooks(loader_test_emit));
    EXPECT_EQ(loader_test_retirement_calls, 8191);
    radiant_loader_begin_document_timing();
    radiant_loader_end_document_timing(&timing);
    EXPECT_EQ(timing.parse_us, 37);
    ASSERT_TRUE(radiant_loader_cleanup_html_script(document));
    EXPECT_EQ(loader_test_document, document);
    loader_test_runtime = runtime;
    EXPECT_EQ(radiant_loader_transform_for_type("math"),
              reinterpret_cast<const LambdaDocumentTransformConfig*>(runtime));
    EXPECT_EQ(ui_context_loader_runtime(host), runtime);
    EXPECT_EQ(loader_test_host, host);
    loader_test_document = nullptr;
    radiant_loader_execute_html_scripts(nullptr, document, nullptr, nullptr, nullptr);
    EXPECT_EQ(loader_test_document, document);
    loader_test_document = nullptr;
    radiant_loader_compile_html_handlers(document);
    EXPECT_EQ(loader_test_document, document);
    loader_runtime_finish_document(document, runtime);
    EXPECT_EQ(loader_test_document, document);
    loader_test_runtime = nullptr;
    radiant_loader_runtime_release(runtime);
    EXPECT_EQ(loader_test_runtime, runtime);
    ASSERT_TRUE(radiant_loader_begin_script(host, nullptr, "source", nullptr, nullptr, 0,
                                            INPUT_RESOURCE_LOCAL_ONLY, &load));
    EXPECT_EQ(load.runtime, runtime);
    radiant_loader_cancel_script(&load, false);
    EXPECT_EQ(load.runtime, nullptr);
    ASSERT_TRUE(radiant_loader_begin_script(host, nullptr, "source", nullptr, nullptr, 0,
                                            INPUT_RESOURCE_LOCAL_ONLY, &load));
    radiant_loader_finish_script(&load, document, nullptr);
    EXPECT_EQ(loader_test_document, document);
    EXPECT_EQ(load.runtime, nullptr);
    EXPECT_TRUE(radiant_loader_prepare_script());
    EXPECT_EQ(loader_test_document, nullptr);
    shared = runtime;
    ASSERT_TRUE(radiant_loader_render_embedded(host, nullptr, &owned, &shared));
    EXPECT_EQ(owned, runtime);
    EXPECT_EQ(shared, nullptr);
}

struct SelectHooks {
    DomElement* selected;
    int reads;
    int writes;
};

TEST(RadiantInputValueTest, FormQueriesUseNativeDocumentOrderAndOwnership) {
    NativeInputDocument fixture;
    DomDocument* document = fixture.document;
    ASSERT_NE(document, nullptr);
    DomElement* root = dom_create_backed_element_identity(document, "div", nullptr, false);
    DomElement* form = dom_create_backed_element_identity(document, "form", nullptr, false);
    DomElement* external = dom_create_backed_element_identity(document, "button", nullptr, false);
    DomElement* nested = dom_create_backed_element_identity(document, "input", nullptr, false);
    DomElement* reset = dom_create_backed_element_identity(document, "button", nullptr, false);
    ASSERT_TRUE(root && form && external && nested && reset);
    document->root = lam::up(root);
    ASSERT_TRUE(form->set_attribute("id", "target"));
    ASSERT_TRUE(external->set_attribute("form", "target"));
    ASSERT_TRUE(external->set_attribute("disabled", ""));
    ASSERT_TRUE(nested->set_attribute("type", "SuBmIt"));
    ASSERT_TRUE(reset->set_attribute("type", "RESET"));
    root->append_child(external);
    root->append_child(form);
    form->append_child(reset);
    form->append_child(nested);
    EXPECT_TRUE(dom_node_is_connected(nested));
    EXPECT_EQ(radiant_form_owner(external), form);
    EXPECT_EQ(radiant_form_owner(nested), form);
    EXPECT_TRUE(radiant_form_is_submit_button(external));
    EXPECT_TRUE(radiant_form_is_submit_button(nested));
    EXPECT_TRUE(radiant_form_is_reset_button(reset));
    EXPECT_STREQ(radiant_form_control_type(nested), "submit");
    EXPECT_EQ(radiant_form_first_submitter(form, false), external);
    EXPECT_EQ(radiant_form_first_submitter(form, true), nested);
    root->remove_child(form);
    EXPECT_FALSE(dom_node_is_connected(nested));
    EXPECT_EQ(radiant_form_first_submitter(form, true), nested);
}

static bool select_hook_read(DomElement* option, void* context) {
    SelectHooks* hooks = static_cast<SelectHooks*>(context);
    hooks->reads++;
    return option == hooks->selected;
}

static void select_hook_write(DomElement* option, bool selected, void* context) {
    SelectHooks* hooks = static_cast<SelectHooks*>(context);
    hooks->writes++;
    option->set_option_selectedness(selected);
}

TEST(RadiantInputValueTest, NormalizesTypesAndSanitizesDatesAndColors) {
    char output[128];
    EXPECT_STREQ(radiant_input_type_normalize("RANGE", output, sizeof(output)), "range");
    EXPECT_STREQ(radiant_input_type_normalize("unknown", output, sizeof(output)), "text");
    ASSERT_TRUE(radiant_input_value_sanitize("date", "2024-02-29", output, sizeof(output)));
    EXPECT_STREQ(output, "2024-02-29");
    ASSERT_TRUE(radiant_input_value_sanitize("date", "2023-02-29", output, sizeof(output)));
    EXPECT_STREQ(output, "");
    ASSERT_TRUE(radiant_input_value_sanitize("color", "#AABBCC", output, sizeof(output)));
    EXPECT_STREQ(output, "#aabbcc");
    ASSERT_TRUE(radiant_input_value_sanitize("color", "invalid", output, sizeof(output)));
    EXPECT_STREQ(output, "#000000");
}

TEST(RadiantInputValueTest, ConvertsAndValidatesNumericValues) {
    double number = 0;
    ASSERT_TRUE(radiant_input_value_as_number("date", "1970-01-02", &number));
    EXPECT_DOUBLE_EQ(number, 86400000);
    RadiantInputValidity validity;
    radiant_input_value_validate("number", "11", "0", "10", "2", &validity);
    EXPECT_TRUE(validity.has_value);
    EXPECT_TRUE(validity.value_valid);
    EXPECT_TRUE(validity.range_overflow);
    EXPECT_TRUE(validity.step_mismatch);
    char output[128];
    ASSERT_TRUE(radiant_input_value_step("number", "4", "0", "10", "2", 2,
                                         output, sizeof(output)));
    EXPECT_STREQ(output, "8");
}

TEST(RadiantInputValueTest, LiveTextBelongsToItsDocument) {
    NativeInputDocument fixture;
    DomDocument* document = fixture.document;
    ASSERT_NE(document, nullptr);
    DomElement* element = DomElement::create(document, "input", nullptr);
    ASSERT_NE(element, nullptr);
    DomDocument* second_document = dom_document_create(fixture.input);
    ASSERT_NE(second_document, nullptr);
    DomElement* second_element = DomElement::create(second_document, "input", nullptr);
    ASSERT_NE(second_element, nullptr);
    EXPECT_EQ(radiant_input_peek_live_value(element), nullptr);
    EXPECT_STREQ(radiant_input_live_value(element), "");
    ASSERT_TRUE(radiant_input_set_live_value(element, "native value"));
    ASSERT_TRUE(radiant_input_set_live_value(second_element, "other document"));
    EXPECT_STREQ(radiant_input_peek_live_value(element), "native value");
    radiant_input_reset_live_value(element);
    EXPECT_STREQ(radiant_input_live_value(element), "");
    dom_document_destroy(document);
    fixture.document = nullptr;
    EXPECT_STREQ(radiant_input_live_value(second_element), "other document");
    dom_document_destroy(second_document);
}

TEST(RadiantInputValueTest, SelectResetUsesNativeOptionTreeAndLiveSelectedness) {
    NativeInputDocument fixture;
    ASSERT_NE(fixture.document, nullptr);
    DomElement* select = dom_create_backed_element_identity(fixture.document, "select", nullptr, false);
    DomElement* first = dom_create_backed_element_identity(fixture.document, "option", nullptr, false);
    DomElement* group = dom_create_backed_element_identity(fixture.document, "optgroup", nullptr, false);
    DomElement* second = dom_create_backed_element_identity(fixture.document, "option", nullptr, false);
    DomElement* last = dom_create_backed_element_identity(fixture.document, "option", nullptr, false);
    DomElement* nested_select = dom_create_backed_element_identity(fixture.document, "select", nullptr, false);
    DomElement* excluded = dom_create_backed_element_identity(fixture.document, "option", nullptr, false);
    DomElement* nested_group = dom_create_backed_element_identity(fixture.document, "optgroup", nullptr, false);
    DomElement* excluded_group = dom_create_backed_element_identity(fixture.document, "option", nullptr, false);
    ASSERT_TRUE(select && first && group && second && last && nested_select && excluded && nested_group && excluded_group);
    fixture.document->root = lam::up(select);
    ASSERT_TRUE(select->append_child(first));
    ASSERT_TRUE(select->append_child(group));
    ASSERT_TRUE(group->append_child(second));
    ASSERT_TRUE(group->append_child(nested_group));
    ASSERT_TRUE(nested_group->append_child(excluded_group));
    ASSERT_TRUE(select->append_child(last));
    ASSERT_TRUE(select->append_child(nested_select));
    ASSERT_TRUE(nested_select->append_child(excluded));
    second->set_option_selectedness(true);
    last->set_option_selectedness(true);
    excluded->set_option_selectedness(true);
    excluded_group->set_option_selectedness(true);
    ASSERT_TRUE(last->set_attribute("disabled", ""));
    int selected_index = -2, option_count = -1;
    ASSERT_TRUE(radiant_select_reset(select, nullptr, &selected_index, &option_count));
    EXPECT_EQ(option_count, 3);
    EXPECT_EQ(selected_index, 2);
    EXPECT_FALSE(dom_option_is_selected(first));
    EXPECT_FALSE(dom_option_is_selected(second));
    EXPECT_TRUE(dom_option_is_selected(last));
    EXPECT_TRUE(dom_option_is_selected(excluded));
    EXPECT_TRUE(dom_option_is_selected(excluded_group));

    ASSERT_TRUE(select->set_attribute("size", "2"));
    last->set_option_selectedness(false);
    ASSERT_TRUE(radiant_select_reset(select, nullptr, &selected_index, &option_count));
    EXPECT_EQ(selected_index, -1);
    EXPECT_FALSE(dom_option_is_selected(first));
    EXPECT_FALSE(dom_option_is_selected(second));
    EXPECT_FALSE(dom_option_is_selected(last));
    SelectHooks hooks = {second, 0, 0};
    RadiantSelectResetOps operations = {select_hook_read, select_hook_write, &hooks};
    RadiantSelectResetOps incomplete = {select_hook_read, nullptr, &hooks};
    EXPECT_FALSE(radiant_select_reset(select, &incomplete, &selected_index, &option_count));
    EXPECT_EQ(hooks.reads, 0);
    ASSERT_TRUE(radiant_select_reset(select, &operations, &selected_index, &option_count));
    EXPECT_EQ(hooks.reads, 3);
    EXPECT_EQ(hooks.writes, 3);
    EXPECT_EQ(selected_index, 1);
    EXPECT_TRUE(dom_option_is_selected(second));
    ASSERT_TRUE(select->set_attribute("multiple", ""));
    first->set_option_selectedness(true);
    EXPECT_FALSE(radiant_select_reset(select, nullptr, &selected_index, &option_count));
    EXPECT_TRUE(dom_option_is_selected(first));
}
