#include <gtest/gtest.h>
#include "../radiant/loader_runtime_host.hpp"
#include "../radiant/view.hpp"
#include "../radiant/radiant.hpp"
#include "../lambda/input/input.hpp"
#include "../lambda/input/css/dom_element.hpp"
#include "../lib/file.h"
#include "../lib/url.h"
#include "../lib/mem.h"

static char* write_test_document_url(const char* path, const char* source) {
    if (file_ensure_dir("temp") != 0 || write_text_file_atomic(path, source) != 0) return nullptr;
    char* absolute_path = file_realpath(path);
    if (!absolute_path) return nullptr;
    char* url = url_from_local_path(absolute_path);
    mem_free(absolute_path);
    return url;
}

TEST(RadiantNoScriptHostTest, EmbeddedMarkdownRejectsAndReleasesBorrowedUrl) {
    // A fresh process has no script provider; the failed loader returns its URL to its caller.
    ASSERT_FALSE(radiant_loader_prepare_script());
    char* url = write_test_document_url("temp/radiant_no_script_host_math.md",
                                        "# Native document\n\nInline math: $x + 1$.\n");
    ASSERT_NE(url, nullptr);
    EXPECT_EQ(load_html_doc(nullptr, url, 800, 600), nullptr);
    mem_free(url);
    // The failed load has already destroyed its URL; the manager must not destroy it again.
    InputManager::destroy_global();
}

TEST(RadiantNoScriptHostTest, PlainMarkdownLoadsAndReleasesWithoutScriptHost) {
    ASSERT_FALSE(radiant_loader_prepare_script());
    char* url = write_test_document_url("temp/radiant_no_script_host_plain.md",
                                        "# Native document\n\nOrdinary text.\n");
    ASSERT_NE(url, nullptr);
    DomDocument* document = load_html_doc(nullptr, url, 800, 600);
    mem_free(url);
    ASSERT_NE(document, nullptr);
    EXPECT_NE(document->root, nullptr);
    free_document(document);
    InputManager::destroy_global();
}

TEST(RadiantNoScriptHostTest, HtmlRendersWithoutExecutingScripts) {
    ASSERT_FALSE(radiant_loader_prepare_script());
    char* url = write_test_document_url("temp/radiant_no_script_host_html.html",
        "<!doctype html><html><body><div id='native' data-value='original'>Native</div>"
        "<script>document.getElementById('native').setAttribute('data-value', 'scripted')</script>"
        "<noscript><span id='fallback'>Fallback</span></noscript>"
        "</body></html>");
    ASSERT_NE(url, nullptr);
    DomDocument* document = load_html_doc(nullptr, url, 800, 600);
    mem_free(url);
    ASSERT_NE(document, nullptr);
    DomElement* element = dom_find_element_by_id(document->root, "native");
    ASSERT_NE(element, nullptr);
    EXPECT_STREQ(element->get_attribute("data-value"), "original");
    EXPECT_TRUE(document->scripting_disabled);
    EXPECT_FALSE(document->html_scripting_enabled);
    EXPECT_NE(dom_find_element_by_id(document->root, "fallback"), nullptr);
    free_document(document);
    InputManager::destroy_global();
}

TEST(RadiantNoScriptHostTest, NativeUiInitializationDoesNotInstallScriptHost) {
    ASSERT_FALSE(radiant_loader_runtime_host_available());
    UiContext ui = {};
    ASSERT_EQ(ui_context_init(&ui, true, 1.0f), 0);
    EXPECT_FALSE(radiant_loader_runtime_host_available());
    EXPECT_TRUE(ui_context_cleanup(&ui));
}
