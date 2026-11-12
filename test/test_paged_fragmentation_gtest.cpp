#include <gtest/gtest.h>
#include "radiant/render.hpp"
#include "radiant/view_tree_model.hpp"
#include "radiant/layout_paged.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lib/file.h"
#include <stdio.h>
#include <stdlib.h>

class PagedFragmentationTest : public ::testing::Test {
protected:
    RenderExportSession session = {};
    void TearDown() override { render_export_session_finish(&session); }
    bool compose(const char* defer, const char* side = "top", const char* extra = "",
        const char* body = "<article id='anchor'><figure id='figure'></figure><div id='text-source' style='height:20px'>Anchored words</div></article><article id='destination'></article><article></article>") {
        char html[2048];
        snprintf(html, sizeof(html),
            "<!doctype html><html><head><style>"
            "@page{size:200px 100px;margin:0}html,body{margin:0}"
            "article{height:40px;break-after:page}article:last-child{break-after:auto}"
            "figure{margin:0;width:60px;height:20px;background:lime;float:%s;float-reference:page;float-defer:%s}%s"
            "</style></head><body>%s</body></html>", side, defer, extra, body);
        return compose_source(html, "html");
    }
    bool compose_source(const char* text, const char* extension) {
        if (file_ensure_dir("temp") != 0) return false;
        char path[128];
        snprintf(path, sizeof(path), "temp/paged-fragmentation.%s", extension);
        if (write_text_file_atomic(path, text) != 0) return false;
        return render_export_session_begin(&session, path,
            0, 0, 200, 100, 1.0f, true, true, 1.0f, false);
    }
    bool compose_columns(const char* fill, const char* contents) {
        char body[1536];
        snprintf(body, sizeof(body),
            "<r:page-document xmlns:r='urn:lambda:radiant:page'><r:master-set>"
            "<r:page-master name='columns' style='size:200px 100px;margin:0'>"
            "<r:region role='body' name='body' style='column-count:2;column-gap:0;column-fill:%s'></r:region>"
            "</r:page-master></r:master-set><r:page-sequence master-reference='columns'>"
            "<r:flow region-name='body' style='orphans:1;widows:1'>%s</r:flow></r:page-sequence></r:page-document>", fill, contents);
        return compose_source(body, "rpd");
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
    void expect_text_coverage(const char* id) {
        DomElement* owner = dom_find_element_by_id(session.document->root, id);
        ASSERT_NE(owner, nullptr);
        expect_text_source_coverage(owner->first_child);
    }
    void expect_text_source_coverage(DomNode* source) {
        ASSERT_TRUE(source && source->is_text());
        size_t length = source->as_text()->length;
        bool* seen = (bool*)calloc(length, sizeof(bool));
        ASSERT_NE(seen, nullptr);
        visit_occurrences(source, [&](const LayoutViewNode* node) {
            if (!node->text_length) return;
            ASSERT_LE(node->text_start + node->text_length, length);
            for (size_t i = node->text_start; i < node->text_start + node->text_length; i++) {
                EXPECT_FALSE(seen[i]); seen[i] = true;
            }
        });
        for (size_t i = 0; i < length; i++) EXPECT_TRUE(seen[i]) << i;
        free(seen);
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
    expect_text_coverage("text-source");
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

TEST_F(PagedFragmentationTest, AutoBodyColumnsKeepSequentialFilling) {
    ASSERT_TRUE(compose_columns("auto", "<div id='a' style='height:20px'></div><div style='height:20px'></div><div id='c' style='height:20px'></div><div style='height:20px'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 0.0f); EXPECT_FLOAT_EQ(rect.y, 40.0f);
}

TEST_F(PagedFragmentationTest, LastBodyPageBalancesAcrossItsEqualColumns) {
    ASSERT_TRUE(compose_columns("balance", "<div id='a' style='height:20px'></div><div style='height:20px'></div><div id='c' style='height:20px'></div><div id='d' style='height:20px'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    const ViewPageStyle* style = session.paged_view->model->pages.get()[0]->style;
    EXPECT_EQ(style->column_count, 2u);
    EXPECT_TRUE(style->balance_columns);
    EXPECT_FLOAT_EQ(style->balanced_height, 40.0f);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 20.0f);
}

TEST_F(PagedFragmentationTest, BalancingRetainsAnIndivisibleBoxAsItsHeightFloor) {
    ASSERT_TRUE(compose_columns("balance", "<div id='large' style='height:70px;break-inside:avoid'></div><div id='small' style='height:30px;break-inside:avoid'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 70.0f);
    size_t count = 0; RdtLogicalRect rect = {};
    occurrence_page("large", VIEW_FRAGMENT_BODY, &count, &rect);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.height, 70.0f);
    occurrence_page("small", VIEW_FRAGMENT_BODY, &count, &rect);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, BalanceRetriesLegalThresholdsWithoutExceedingThePageBudget) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:30px;break-inside:avoid'></div><div style='height:30px;break-inside:avoid'></div><div id='tail' style='height:30px;break-inside:avoid'></div>"));
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    ViewEnvironment environment = session.paged_view->model->environment;
    ViewTree* limited = view_tree_secondary_create(session.document, &environment);
    ASSERT_NE(limited, nullptr);
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 1;
    PagedLayoutDiagnostic diagnostic = {};
    ASSERT_EQ(layout_secondary_view(limited, &options, &diagnostic), TYPESET_OK) << diagnostic.reason;
    EXPECT_EQ(limited->model->page_count, 1u);
    EXPECT_FLOAT_EQ(limited->model->pages.get()[0]->style->balanced_height, 60.0f);
    size_t count = 0; RdtLogicalRect rect = {};
    occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, BalancePassBudgetFailureKeepsThePublishedEdition) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:20px'></div><div style='height:20px'></div><div id='tail' style='height:20px'></div><div style='height:20px'></div>"));
    ViewEnvironment environment = session.paged_view->model->environment;
    ViewTree* limited = view_tree_secondary_create(session.document, &environment);
    ASSERT_NE(limited, nullptr);
    PagedLayoutOptions options = paged_layout_options_default(); options.max_reference_passes = 1;
    PagedLayoutDiagnostic diagnostic = {};
    EXPECT_EQ(layout_secondary_view(limited, &options, &diagnostic), TYPESET_BUDGET_EXHAUSTED);
    EXPECT_FALSE(limited->model->committed);
    EXPECT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 40.0f);
}

TEST_F(PagedFragmentationTest, BalancingOnlyShortensTheLastPhysicalBodyPage) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:200px'></div><div style='height:20px'></div><div style='height:20px'></div><div id='tail' style='height:20px'></div><div style='height:20px'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 2u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 0.0f);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[1]->style->balanced_height, 40.0f);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, BalancedColumnsKeepTheOriginalViewportHeightReference) {
    ASSERT_TRUE(compose_columns("balance", "<div id='half-height' style='height:50vh'></div><div style='height:30px'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    DomElement* owner = dom_find_element_by_id(session.document->root, "half-height");
    ASSERT_NE(owner, nullptr);
    float total = 0.0f;
    visit_occurrences(owner, [&](const LayoutViewNode* node) { if (node->paint_box) total += node->rect.height; });
    EXPECT_FLOAT_EQ(total, 50.0f);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 40.0f);
}

TEST_F(PagedFragmentationTest, BalancedTextRetainsWidowsAndEveryAuthoredByte) {
    ASSERT_TRUE(compose_columns("balance", "<p style='margin:0;font:10px/20px monospace;orphans:2;widows:2'><span id='a'>A</span><br/><span id='b'>B</span><br/><span id='c'>C</span><br/><span id='d'>D</span><br/><span id='e'>E</span></p>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    for (const char* id : {"a", "b", "c", "d", "e"}) expect_text_coverage(id);
    DomElement* tail = dom_find_element_by_id(session.document->root, "e");
    visit_occurrences(tail->first_child, [&](const LayoutViewNode* node) {
        if (node->glyph_run) EXPECT_FLOAT_EQ(node->rect.x, 100.0f);
    });
}

class PagedRowTest : public PagedFragmentationTest, public ::testing::WithParamInterface<const char*> {
protected:
    bool compose_rows(const char* extra = "", const char* body =
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div></div><div id='tail' style='height:20px'>Tail</div>") {
        char css[1024];
        snprintf(css, sizeof(css), "#rows{display:%s;flex-wrap:wrap;grid-template-columns:100px 100px;grid-auto-rows:60px}#rows>div{width:100px;height:60px;flex:none}%s", GetParam(), extra);
        return compose("0", "top", css, body);
    }
};

TEST_P(PagedRowTest, WholeRowsMoveTogetherAndAdvanceTheFollowingFlow) {
    ASSERT_TRUE(compose_rows());
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f); EXPECT_FLOAT_EQ(rect.height, 60.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f); EXPECT_FLOAT_EQ(rect.height, 60.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.y, 60.0f);
    for (const char* id : {"a", "b", "c", "d", "e", "f", "tail"}) expect_text_coverage(id);
}

TEST_P(PagedRowTest, ExplicitRowBreakUsesTheSamePhysicalPageBoundary) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:20px}#rows>div{height:20px}#c{break-before:page}"));
    ASSERT_EQ(session.paged_view->model->page_count, 2u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 20.0f);
}

TEST_P(PagedRowTest, ContainerAvoidanceStillMovesTheWholeFittingGroup) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:20px;break-inside:avoid}#rows>div{height:20px}",
        "<div style='height:60px'></div><div id='rows'><div>A</div><div>B</div><div id='c'>C</div><div>D</div><div id='e'>E</div><div>F</div></div><div id='tail' style='height:20px'>Tail</div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 2u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("rows", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f); EXPECT_FLOAT_EQ(rect.height, 60.0f);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 20.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 60.0f);
}

INSTANTIATE_TEST_SUITE_P(FormattingContexts, PagedRowTest, ::testing::Values("flex", "grid"));

TEST_P(PagedRowTest, AnAvoidedBoundaryMovesItsFittingRowPrefixTogether) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:30px}#rows>div{height:30px}#c{break-after:avoid}",
        "<div style='height:20px'></div><div id='rows'><div id='a'>A</div><div>B</div><div id='c'>C</div><div>D</div><div id='e'>E</div><div>F</div></div><div id='tail' style='height:20px'>Tail</div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 2u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 30.0f);
    expect_text_coverage("c"); expect_text_coverage("e");
}

TEST_P(PagedRowTest, RowOrderFollowsTheFormatterRatherThanDomOrder) {
    ASSERT_TRUE(compose_rows("#a{order:1}"));
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    expect_text_coverage("a");
}

TEST_P(PagedRowTest, AnonymousTextMovesAfterTheFormatterScratchRetires) {
    ASSERT_TRUE(compose_rows("", "<div id='rows'><div>A</div><div>B</div>Direct<div id='d'>D</div><div>E</div><div>F</div></div><div id='tail' style='height:20px'>Tail</div>"));
    DomElement* rows = dom_find_element_by_id(session.document->root, "rows");
    ASSERT_NE(rows, nullptr);
    DomNode* text = rows->first_child;
    while (text && !text->is_text()) text = text->next_sibling;
    ASSERT_NE(text, nullptr);
    expect_text_source_coverage(text);
    size_t pieces = 0;
    visit_occurrences(text, [&](const LayoutViewNode* node) {
        if (!node->glyph_run) return;
        EXPECT_EQ(page_of(node), 2u); pieces++;
    });
    EXPECT_GT(pieces, 0u);
}

TEST_P(PagedRowTest, OversizedRowsKeepEverySliceWithoutInventingEjectPages) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:120px}#rows>div{height:120px}"));
    EXPECT_EQ(session.paged_view->model->page_count, 4u);
    for (const char* id : {"a", "b", "c", "d", "e", "f"}) {
        DomElement* source = dom_find_element_by_id(session.document->root, id);
        ASSERT_NE(source, nullptr); float total = 0.0f;
        visit_occurrences(source, [&](const LayoutViewNode* node) { if (node->paint_box) total += node->rect.height; });
        EXPECT_FLOAT_EQ(total, 120.0f); expect_text_coverage(id);
    }
}

TEST_P(PagedRowTest, AForcedLastRowBreakPropagatesToTheNextFlowBox) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:20px}#rows>div{height:20px}#e{break-after:page}"));
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, GridRowSpansKeepTheirConnectedTracksTogether) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:grid;grid-template-columns:100px 100px;grid-auto-rows:40px}#span{grid-row:span 2}",
        "<div id='rows'><div id='span'>Span</div><div>A</div><div>B</div><div id='next'>Next</div><div>C</div></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 2u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("span", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.height, 80.0f);
    EXPECT_EQ(occurrence_page("next", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    expect_text_coverage("span"); expect_text_coverage("next");
}

TEST_P(PagedRowTest, RowGapsDoNotLeaveLeadingSpaceOnTheNextSheet) {
    ASSERT_TRUE(compose_rows("#rows{row-gap:10px}"));
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_P(PagedRowTest, RowsShareTheFinalBodyColumnBalancingFits) {
    char contents[1024];
    snprintf(contents, sizeof(contents),
        "<div id='rows' style='display:%s;flex-wrap:wrap;grid-template-columns:50px 50px;grid-auto-rows:30px'>"
        "<div style='width:50px;height:30px;flex:none'>A</div><div style='width:50px;height:30px;flex:none'>B</div>"
        "<div id='c' style='width:50px;height:30px;flex:none'>C</div><div style='width:50px;height:30px;flex:none'>D</div>"
        "<div id='e' style='width:50px;height:30px;flex:none'>E</div><div style='width:50px;height:30px;flex:none'>F</div></div>", GetParam());
    ASSERT_TRUE(compose_columns("balance", contents));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    size_t count = 0; RdtLogicalRect rect = {};
    occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect);
    EXPECT_FLOAT_EQ(rect.x, 0.0f); EXPECT_FLOAT_EQ(rect.y, 30.0f);
    occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    expect_text_coverage("c"); expect_text_coverage("e");
}

TEST_P(PagedRowTest, FailedRowAdmissionLeavesTheRetainedPagePixelsUnchanged) {
    ASSERT_TRUE(compose_rows("#rows>div{background:lime}"));
    ViewEnvironment environment = session.paged_view->model->environment;
    ViewTree* limited = view_tree_secondary_create(session.document, &environment);
    ASSERT_NE(limited, nullptr);
    ImageSurface* before = render_secondary_page_snapshot(session.paged_view, 2);
    EXPECT_NE(before, nullptr);
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    PagedLayoutDiagnostic diagnostic = {};
    EXPECT_EQ(layout_secondary_view(limited, &options, &diagnostic), TYPESET_BUDGET_EXHAUSTED);
    EXPECT_FALSE(limited->model->committed);
    ImageSurface* after = render_secondary_page_snapshot(session.paged_view, 2);
    EXPECT_NE(after, nullptr);
    if (before && after) {
        EXPECT_EQ(before->width, after->width); EXPECT_EQ(before->height, after->height); EXPECT_EQ(before->pitch, after->pitch);
        if (before->width == after->width && before->height == after->height && before->pitch == after->pitch)
            EXPECT_EQ(memcmp(before->pixels, after->pixels, (size_t)before->pitch * before->height), 0);
    }
    image_surface_destroy(before); image_surface_destroy(after);
}

TEST_F(PagedFragmentationTest, NegativeDeferralCountsBackFromTheFlowEnd) {
    ASSERT_TRUE(compose("-2"));
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 2u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, LastDeferralSelectsTheLastExistingBodyPage) {
    ASSERT_TRUE(compose("last"));
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 3u);
    EXPECT_EQ(count, 1u); EXPECT_EQ(session.paged_view->model->page_count, 3u);
}

TEST_F(PagedFragmentationTest, AnOutOfRangeNegativeOffsetKeepsItsOwnAnchor) {
    ASSERT_TRUE(compose("-99"));
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 1u);
    EXPECT_EQ(count, 1u); EXPECT_EQ(session.paged_view->model->page_count, 3u);
}

TEST_F(PagedFragmentationTest, EndRelativeFloatDiscoveryDoesNotSpendAnExtraSheet) {
    ASSERT_TRUE(compose("-3", "top", "#anchor{height:1px}figure{height:80px}",
        "<article id='anchor'><figure id='figure'></figure><div style='font:1px/1px monospace;height:1px'>Anchor</div></article><article></article><article></article>"));
    ViewEnvironment environment = session.paged_view->model->environment;
    ViewTree* limited = view_tree_secondary_create(session.document, &environment);
    ASSERT_NE(limited, nullptr);
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 3;
    PagedLayoutDiagnostic diagnostic = {};
    ASSERT_EQ(layout_secondary_view(limited, &options, &diagnostic), TYPESET_OK) << diagnostic.reason;
    EXPECT_EQ(limited->model->page_count, 3u);
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 1u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, AFixedWidthFlowAncestorKeepsItsMeasureOnTheNamedSheet) {
    ASSERT_TRUE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div id='outer'><div id='fixed' style='width:150px'><article>A</article><article id='wide' style='page:wide'><div id='half' style='width:50%;height:10px'></div></article></div><article id='tail'>Tail</article></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("wide", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 150.0f);
    EXPECT_EQ(occurrence_page("half", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 75.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.width, 200.0f); expect_text_coverage("tail");
    DomElement* fixed = dom_find_element_by_id(session.document->root, "fixed");
    visit_occurrences(fixed, [&](const LayoutViewNode* node) { if (node->paint_box) EXPECT_FLOAT_EQ(node->rect.width, 150.0f); });
}

TEST_F(PagedFragmentationTest, AnAutoAncestorResumesAtTheNewMeasureAfterAFixedWidthChild) {
    ASSERT_TRUE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div id='outer'><div style='width:150px'><article>A</article><article style='page:wide'>Wide</article></div><article id='tail' style='page:wide'><div id='half' style='width:50%;height:10px'></div></article></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 2u);
    size_t count = 0; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 40.0f); EXPECT_FLOAT_EQ(rect.width, 300.0f);
    occurrence_page("half", VIEW_FRAGMENT_BODY, &count, &rect);
    EXPECT_FLOAT_EQ(rect.width, 150.0f);
}

TEST_F(PagedFragmentationTest, AComputedNegativeDeferralUsesTheResolvedPageOffset) {
    ASSERT_TRUE(compose("var(--delay)", "top", "figure{--delay:-2}"));
    size_t count = 0;
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count), 2u);
    EXPECT_EQ(count, 1u);
}

TEST_F(PagedFragmentationTest, EndRelativeDestinationsBeforeTheAnchorFailExplicitly) {
    EXPECT_FALSE(compose("-3", "top", "", "<article>A</article><article><figure id='figure'></figure>Anchor</article><article>C</article>"));
}

TEST_F(PagedFragmentationTest, PercentageContainingChainsRemainOutsideFixedWidthContinuation) {
    EXPECT_FALSE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div style='width:50%'><article>A</article><article style='page:wide'>Wide</article></div>"));
}

TEST_F(PagedFragmentationTest, IndependentFormattingContextsDoNotReuseTheOrdinaryWidthChain) {
    EXPECT_FALSE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div style='display:flow-root;width:150px'><article>A</article><article style='page:wide'>Wide</article></div>"));
}
