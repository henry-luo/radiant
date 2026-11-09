#include <gtest/gtest.h>
#include "radiant/view.hpp"
#include "radiant/radiant.hpp"
#include "lambda/input/input.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lib/mem.h"
#include "lib/url.h"

class RadiantNavigationTest : public ::testing::Test {
protected:
    DomDocument* document = nullptr;

    void SetUp() override {
        Url* base = url_parse("file:///radiant/navigation.html?mode=one#old");
        ASSERT_NE(base, nullptr);
        document = load_html_source_doc(
            "<!doctype html><div id='result' data-value='retained'>Native document</div>",
            800, 600, nullptr, false, base);
        url_destroy(base);
        ASSERT_NE(document, nullptr);
    }

    void TearDown() override {
        if (document) free_document(document);
        InputManager::destroy_global();
    }

    void retain_fragment(const char* target, const char* expected) {
        Url* carrier = document->url;
        DomElement* root = document->root;
        Input* input = document->input;
        Pool* pool = document->document_pool;
        document->pending_navigation_url = mem_strdup(target, MEM_CAT_DOM);
        EXPECT_EQ(document_resolve_navigation_redirect(document), nullptr);
        EXPECT_EQ(document->pending_navigation_url, nullptr);
        EXPECT_EQ(document->url.get(), carrier);
        EXPECT_EQ(document->root.get(), root);
        EXPECT_EQ(document->input, input);
        EXPECT_EQ(document->document_pool.get(), pool);
        EXPECT_STREQ(url_get_href(carrier), expected);
        DomElement* result = dom_find_element_by_id(document->root, "result");
        ASSERT_NE(result, nullptr);
        EXPECT_STREQ(result->get_attribute("data-value"), "retained");
        EXPECT_EQ(document_resolve_navigation_redirect(document), nullptr);
    }

    void expect_redirect(const char* target, const char* expected) {
        Url* carrier = document->url;
        document->pending_navigation_url = mem_strdup(target, MEM_CAT_DOM);
        Url* redirect = document_resolve_navigation_redirect(document);
        ASSERT_NE(redirect, nullptr);
        EXPECT_STREQ(url_get_href(redirect), expected);
        EXPECT_EQ(document->url.get(), carrier);
        EXPECT_STREQ(url_get_href(carrier), "file:///radiant/navigation.html?mode=one#old");
        url_destroy(redirect);
    }
};

TEST_F(RadiantNavigationTest, RelativeFragmentRetainsDocumentAndUrlCarrier) {
    retain_fragment("#result", "file:///radiant/navigation.html?mode=one#result");
}

TEST_F(RadiantNavigationTest, AbsoluteFragmentRetainsDocumentAndUrlCarrier) {
    retain_fragment("file:///radiant/navigation.html?mode=one#result",
                    "file:///radiant/navigation.html?mode=one#result");
}

TEST_F(RadiantNavigationTest, RepeatedFragmentIsConsumed) {
    retain_fragment("#old", "file:///radiant/navigation.html?mode=one#old");
}

TEST_F(RadiantNavigationTest, EmptyFragmentIsSameDocument) {
    retain_fragment("#", "file:///radiant/navigation.html?mode=one#");
}

TEST_F(RadiantNavigationTest, QueryChangeStillRedirects) {
    expect_redirect("?mode=two#result", "file:///radiant/navigation.html?mode=two#result");
}

TEST_F(RadiantNavigationTest, OtherDocumentFragmentStillRedirects) {
    expect_redirect("next.html#result", "file:///radiant/next.html#result");
}

TEST_F(RadiantNavigationTest, SameUrlWithoutFragmentStillReloads) {
    expect_redirect("navigation.html?mode=one", "file:///radiant/navigation.html?mode=one");
}

TEST_F(RadiantNavigationTest, NoPendingNavigationRetainsDocument) {
    EXPECT_EQ(document_resolve_navigation_redirect(document), nullptr);
    EXPECT_EQ(document_resolve_navigation_redirect(nullptr), nullptr);
}
