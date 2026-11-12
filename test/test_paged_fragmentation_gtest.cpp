#include <gtest/gtest.h>
#include "radiant/render.hpp"
#include "radiant/view_tree_model.hpp"
#include "radiant/layout_paged.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lib/file.h"
#include <stdio.h>

class PagedFragmentationTest : public ::testing::Test {
protected:
    RenderExportSession session = {};
    void TearDown() override { render_export_session_finish(&session); }
    bool compose(const char* defer, const char* side = "top", const char* extra = "",
        const char* body = "<article id='anchor'><figure id='figure'></figure><div id='text-source' style='height:20px'>Anchored words</div></article><article id='destination'></article><article></article>") {
        if (file_ensure_dir("temp") != 0) return false;
        char html[2048];
        snprintf(html, sizeof(html),
            "<!doctype html><html><head><style>"
            "@page{size:200px 100px;margin:0}html,body{margin:0}"
            "article{height:40px;break-after:page}article:last-child{break-after:auto}"
            "figure{margin:0;width:60px;height:20px;background:lime;float:%s;float-reference:page;float-defer:%s}%s"
            "</style></head><body>%s</body></html>", side, defer, extra, body);
        if (write_text_file_atomic("temp/paged-fragmentation.html", html) != 0) return false;
        return render_export_session_begin(&session, "temp/paged-fragmentation.html",
            0, 0, 200, 100, 1.0f, true, true, 1.0f, false);
    }
    uint32_t occurrence_page(const char* id, ViewFragmentRole role, size_t* count, RdtLogicalRect* rect = nullptr) {
        DomElement* source = dom_find_element_by_id(session.document->root, id);
        EXPECT_NE(source, nullptr);
        uint32_t number = 0;
        *count = 0;
        visit_occurrences(source, [&](const LayoutViewNode* node) {
            if (node->role != role || !node->paint_box) return;
            (*count)++;
            if (rect) *rect = node->rect;
            number = page_of(node);
        });
        return number;
    }
    template<typename Visitor> void visit_occurrences(DomNode* source, Visitor visitor) {
        ViewNodeState* state = view_tree_node_state(session.paged_view, source, false);
        for (const LayoutViewNode* node = state ? state->first_occurrence.get() : nullptr;
                node; node = node->next_occurrence) visitor(node);
    }
    static uint32_t page_of(const LayoutViewNode* node) {
        while (node && node->kind != LAYOUT_VIEW_PAGE) node = node->parent;
        return node ? ((const ViewPageBox*)node)->page_number : 0;
    }
};

TEST_F(PagedFragmentationTest, ZeroDeferralKeepsFigureBesideItsAnchor) {
    ASSERT_TRUE(compose("0"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 1u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, DeferredFigureMovesWithoutItsAnchor) {
    ASSERT_TRUE(compose("1"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(occurrence_page("anchor", VIEW_FRAGMENT_BODY, &count), 1u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, TwoPageOffsetSelectsThirdSheet) {
    ASSERT_TRUE(compose("2"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 3u);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
}

TEST_F(PagedFragmentationTest, ExcessiveOffsetClampsWithoutInventingSheets) {
    ASSERT_TRUE(compose("4294967295"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 3u);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
}

TEST_F(PagedFragmentationTest, BottomFloatUsesDestinationSheetBottom) {
    ASSERT_TRUE(compose("1", "bottom"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.y, 80.0f);
    EXPECT_FLOAT_EQ(rect.height, 20.0f);
}

TEST_F(PagedFragmentationTest, TopFloatReservesSpaceAheadOfDestinationBody) {
    ASSERT_TRUE(compose("1"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("destination", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.y, 20.0f);
    EXPECT_EQ(occurrence_page("anchor", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, OneSheetFlowClampsOffsetAtItsOwnEnd) {
    ASSERT_TRUE(compose("2", "top", "", "<article id='anchor'><figure id='figure'></figure><div style='height:20px'></div></article>"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 1u);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(session.paged_view->model->page_count, 1u);
}

TEST_F(PagedFragmentationTest, DeferredFigureRetainsEveryAnchoredTextByteOnce) {
    ASSERT_TRUE(compose("2"));
    ASSERT_NE(session.paged_view.get(), nullptr);
    DomElement* owner = dom_find_element_by_id(session.document->root, "text-source");
    ASSERT_NE(owner, nullptr);
    DomNode* text = owner->first_child;
    ASSERT_NE(text, nullptr);
    bool seen[14] = {};
    visit_occurrences(text, [&](const LayoutViewNode* node) {
        if (!node->text_length) return;
        ASSERT_LE(node->text_start + node->text_length, sizeof(seen));
        for (size_t j = node->text_start; j < node->text_start + node->text_length; j++) {
            EXPECT_FALSE(seen[j]); seen[j] = true;
        }
    });
    for (bool present : seen) EXPECT_TRUE(present);
}

TEST_F(PagedFragmentationTest, FiguresSharingDestinationKeepTheirSourceOrder) {
    ASSERT_TRUE(compose("1", "top", "", "<article id='anchor'><figure id='figure'></figure><figure id='second'></figure><div style='height:20px'></div></article><article></article><article></article>"));
    size_t count = 0;
    RdtLogicalRect first = {}, second = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &first), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_EQ(occurrence_page("second", VIEW_FRAGMENT_FLOAT, &count, &second), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(first.y, 0.0f);
    EXPECT_FLOAT_EQ(second.y, first.y + first.height);
}

TEST_F(PagedFragmentationTest, SidedBreakDoesNotPublishDeferredFigureOnBlankSheet) {
    ASSERT_TRUE(compose("1", "top", "#destination{break-before:right}"));
    ASSERT_EQ(session.paged_view->model->page_count, 4u);
    EXPECT_TRUE(session.paged_view->model->pages.get()[1]->blank);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 3u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, FailedPageBudgetKeepsPreviouslyPublishedEdition) {
    ASSERT_TRUE(compose("1"));
    ViewEnvironment environment = session.paged_view->model->environment;
    ViewTree* limited = view_tree_secondary_create(session.document, &environment);
    ASSERT_NE(limited, nullptr);
    PagedLayoutOptions options = paged_layout_options_default();
    options.max_pages = 2;
    PagedLayoutDiagnostic diagnostic = {};
    EXPECT_EQ(layout_secondary_view(limited, &options, &diagnostic), TYPESET_BUDGET_EXHAUSTED);
    EXPECT_EQ(limited->model->page_count, 0u);
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 2u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, FigureTallerThanEveryAdmittedAreaFailsWithoutTruncation) {
    EXPECT_FALSE(compose("1", "top", "figure{height:110px}"));
}

TEST_F(PagedFragmentationTest, FigureCanWaitForALargerDestinationArea) {
    ASSERT_TRUE(compose("1", "top", "@page tall{size:200px 200px;margin:0}#destination{page:tall}figure{height:110px}"));
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.height, 110.0f);
}

TEST_F(PagedFragmentationTest, PercentageHeightUsesDestinationPageArea) {
    ASSERT_TRUE(compose("1", "top", "@page tall{size:200px 200px;margin:0}#destination{page:tall}figure{height:25%}"));
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.height, 50.0f);
    EXPECT_EQ(occurrence_page("destination", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 50.0f);
}

TEST_F(PagedFragmentationTest, CustomPropertyDeferralUsesTheComputedDeclaration) {
    ASSERT_TRUE(compose("var(--delay)", "top", "figure{--delay:1}"));
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 2u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, ComputedPageNameRetainsDestinationSizing) {
    ASSERT_TRUE(compose("1", "top", "@page tall{size:200px 200px;margin:0}#destination{--sheet:tall;page:var(--sheet)}figure{height:25%}"));
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.height, 50.0f);
}

TEST_F(PagedFragmentationTest, NestedNamedPageReturnsToPortraitInsideOpenFlow) {
    ASSERT_TRUE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div id='outer'><article id='anchor'>A</article><article id='wide' style='page:wide'><div id='half' style='width:50%;height:10px;background:blue'></div></article><article id='tail'>C</article></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->node.rect.width, 200.0f);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[1]->node.rect.width, 300.0f);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[2]->node.rect.width, 200.0f);
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("wide", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 300.0f);
    EXPECT_EQ(occurrence_page("half", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 150.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.width, 200.0f);
}

TEST_F(PagedFragmentationTest, NestedNamedPageRetainsEnclosingPaddingAndFragmentWidths) {
    ASSERT_TRUE(compose("0", "top", "@page wide{size:300px 100px;margin:0}#outer{padding:0 10px}",
        "<div id='outer'><article>A</article><article id='wide' style='page:wide'><div id='half' style='width:50%;height:10px;background:blue'></div></article><article>C</article></div>"));
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("wide", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 280.0f);
    EXPECT_EQ(occurrence_page("half", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 140.0f);
    DomElement* outer = dom_find_element_by_id(session.document->root, "outer");
    ASSERT_NE(outer, nullptr);
    count = 0;
    visit_occurrences(outer, [&](const LayoutViewNode* node) {
        if (!node->paint_box || node->role != VIEW_FRAGMENT_BODY) return;
        uint32_t page = page_of(node);
        ASSERT_GT(page, 0u);
        ASSERT_LE(page, session.paged_view->model->page_count);
        EXPECT_FLOAT_EQ(node->rect.width, session.paged_view->model->pages.get()[page - 1]->content_rect.width);
        count++;
    });
    EXPECT_EQ(count, 3u);
}

TEST_F(PagedFragmentationTest, AncestorResumesAtTheMeasureSelectedByItsNestedChild) {
    ASSERT_TRUE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div id='outer'><div id='inner'><article>A</article><article style='page:wide;break-after:auto'>B</article></div><article id='continued' style='page:wide'><div id='half' style='width:50%;height:10px'></div></article><article id='tail'>C</article></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0;
    RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("continued", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.y, 40.0f);
    EXPECT_FLOAT_EQ(rect.width, 300.0f);
    EXPECT_EQ(occurrence_page("half", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 150.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.width, 200.0f);
}
