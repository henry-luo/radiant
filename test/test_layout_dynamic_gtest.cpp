#include <gtest/gtest.h>
#include "radiant/radiant.hpp"
#include "radiant/layout.hpp"
#include "radiant/tagged.hpp"
#include "radiant/view.hpp"
#include "lambda/input/input.hpp"
#include "lib/file.h"
#include "lib/mem.h"
#include "lib/strbuf.h"
#include "lib/url.h"

class DynamicLayoutTest : public ::testing::Test {
protected:
    UiContext ui = {};
    DomDocument* document = nullptr;

    void SetUp() override {
        ASSERT_EQ(file_ensure_dir("temp"), 0);
        ASSERT_EQ(ui_context_init(&ui, true, 1.0f), 0);
        ui.viewport_width = 1200;
        ui.viewport_height = 800;
    }

    void TearDown() override {
        ui.document = nullptr;
        if (document) free_document(document);
        EXPECT_TRUE(ui_context_cleanup(&ui));
        InputManager::destroy_global();
    }

    void load(const char* source) {
        ASSERT_EQ(write_text_file_atomic("temp/radiant_dynamic_layout.html", source), 0);
        char* path = file_realpath("temp/radiant_dynamic_layout.html");
        ASSERT_NE(path, nullptr);
        char* url = url_from_local_path(path);
        mem_free(path);
        ASSERT_NE(url, nullptr);
        document = load_html_doc(nullptr, url, 1200, 800);
        mem_free(url);
        ASSERT_NE(document, nullptr);
        ui.document = lam::up(document);
        layout_html_doc(&ui, document, false);
    }

    DomElement* element(const char* id) {
        return dom_find_element_by_id(document->root, id);
    }

    void check_alignment(bool flipped) {
        for (int i = 0; i < 16; i++) {
            char id[24];
            snprintf(id, sizeof(id), "child%d", i);
            DomElement* child = element(id);
            ASSERT_NE(child, nullptr);
            ASSERT_NE(child->blk, nullptr);
            bool parent_rtl = (i % 4 >= 2) != flipped;
            bool right = i < 8 ? (i >= 4) != parent_rtl : i >= 12;
            SCOPED_TRACE(id);
            EXPECT_EQ(child->block()->text_align, right ? CSS_VALUE_RIGHT : CSS_VALUE_LEFT);
            DomNode* text = child->first_child;
            ASSERT_NE(text, nullptr);
            EXPECT_GT(text->width, 0.0f);
            EXPECT_NEAR(text->x, right ? child->width - text->width : 0.0f, 1.0f);
        }
    }

    void load_alignment_matrix(const char* child_class, const char* parent_width) {
        StrBuf* html = strbuf_create("<!doctype html><style>"
            ".start{text-align:start}.end{text-align:end}.left{text-align:left}"
            ".right{text-align:right}.match{text-align:match-parent}.center{text-align:center}"
            "</style><body>");
        const char* align[] = {"start", "end", "left", "right"};
        for (int i = 0; i < 16; i++) {
            strbuf_append_format(html,
                "<div id='parent%d' class='%s' dir='%s' style='%s'>"
                "<div id='child%d' class='%s' dir='%s'>Text</div></div>",
                i, align[i / 4], i % 4 >= 2 ? "rtl" : "ltr", parent_width,
                i, child_class, i % 2 ? "rtl" : "ltr");
        }
        load(html->str);
        strbuf_free(html);
    }
};

TEST_F(DynamicLayoutTest, MatchParentTracksParentDirection) {
    load_alignment_matrix("match", "");
    ASSERT_NE(document, nullptr);
    check_alignment(false);
    for (int i = 0; i < 16; i++) {
        char id[24];
        snprintf(id, sizeof(id), "parent%d", i);
        ASSERT_TRUE(element(id)->set_attribute("dir", i % 4 >= 2 ? "ltr" : "rtl"));
    }
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    check_alignment(true);
}

TEST_F(DynamicLayoutTest, MatchParentAfterClassAndWidthChanges) {
    load_alignment_matrix("center", "width:50%");
    ASSERT_NE(document, nullptr);
    for (int i = 0; i < 16; i++) {
        char id[24];
        snprintf(id, sizeof(id), "child%d", i);
        ASSERT_TRUE(element(id)->set_attribute("class", "match"));
    }
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    check_alignment(false);
    for (int i = 0; i < 16; i++) {
        char id[24];
        snprintf(id, sizeof(id), "parent%d", i);
        ASSERT_TRUE(element(id)->remove_attribute("style"));
    }
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    check_alignment(false);
}

TEST_F(DynamicLayoutTest, SuppressedTableMarkersDoNotSplitInlineContent) {
    load("<!doctype html><body><div><span id='wrapper'><span>a</span>"
         "<span id='removed' style='display:table-column-group'></span> "
         "<span id='column' style='display:table-column-group'></span>"
         "<span id='caption' style='display:table-caption'></span>"
         "<span id='tail'>bc d</span></span></div></body>");
    ASSERT_NE(document, nullptr);
    for (int pass = 0; pass < 2; pass++) {
        const char* markers[] = {"column", "caption"};
        for (const char* id : markers) {
            SCOPED_TRACE(id);
            DomElement* marker = element(id);
            ASSERT_NE(marker, nullptr);
            EXPECT_FLOAT_EQ(marker->width, 0.0f);
            EXPECT_FLOAT_EQ(marker->height, 0.0f);
        }
        DomElement* tail = element("tail");
        ASSERT_NE(tail, nullptr);
        EXPECT_GT(tail->width, 0.0f);
        EXPECT_NEAR(tail->y, element("wrapper")->first_child->y, 1.0f);
        print_view_tree_json(lam::view_require_element(document->view_tree->root), nullptr,
                             "temp/radiant_dynamic_layout.json");
        char* snapshot = read_text_file("temp/radiant_dynamic_layout.json");
        ASSERT_NE(snapshot, nullptr);
        EXPECT_EQ(strstr(snapshot, "::anon-"), nullptr);
        EXPECT_NE(strstr(snapshot, "table-column-group"), nullptr);
        EXPECT_NE(strstr(snapshot, "bc d"), nullptr);
        mem_free(snapshot);
        if (pass == 0) {
            DomElement* removed = element("removed");
            ASSERT_TRUE(removed->parent->as_element()->remove_child(removed));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, true);
        }
    }
}

TEST_F(DynamicLayoutTest, WhitespaceBeforeOrphanedTableContent) {
    load("<!doctype html><body style='font-size:32px'><div>"
         "<span id='actual'>a <span style='display:table-cell'>b</span>c d</span>"
         "</div><div><span id='reference'>a bc d</span></div></body>");
    ASSERT_NE(document, nullptr);
    EXPECT_NEAR(element("actual")->width, element("reference")->width, 0.2f);
}

TEST_F(DynamicLayoutTest, GeneratedTableCellRemainsInItsInlineRun) {
    load("<!doctype html><style>#actual:after{content:' cd';display:table-cell}</style>"
         "<body style='font:32px monospace;white-space:pre-wrap'><div style='width:10em'>"
         "<span id='removed' style='float:left;width:80%;height:.5em'></span>"
         "<span id='actual'>a <script>ignored</script> <span style='display:table-cell'>b</span></span>"
         "</div><div><span id='reference'>a  b cd</span></div></body>");
    ASSERT_NE(document, nullptr);
    DomElement* removed = element("removed");
    ASSERT_TRUE(removed->parent->as_element()->remove_child(removed));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    EXPECT_NEAR(element("actual")->width, element("reference")->width, 0.2f);
}

TEST_F(DynamicLayoutTest, GeneratedTableCellsKeepOriginatingCountersAcrossReflow) {
    load("<!doctype html><style>#actual{counter-reset:part 7}"
         "#actual:before,#actual:after{content:counter(part);counter-increment:part;display:table-cell}"
         "</style><body><span id='actual'><span style='display:table-cell'>b</span></span></body>");
    ASSERT_NE(document, nullptr);
    for (int pass = 0; pass < 2; pass++) {
        DomElement* actual = element("actual");
        ASSERT_NE(actual->pseudo, nullptr);
        DomElement* generated[] = {actual->pseudo->before, actual->pseudo->after};
        const char* expected[] = {"8", "9"};
        for (int i = 0; i < 2; i++) {
            ASSERT_NE(generated[i], nullptr);
            ASSERT_NE(generated[i]->first_child, nullptr);
            ASSERT_TRUE(generated[i]->first_child->is_text());
            EXPECT_STREQ(generated[i]->first_child->as_text()->text, expected[i]);
            EXPECT_GT(generated[i]->width, 0.0f);
        }
        if (pass == 0) {
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, true);
        }
    }
}
