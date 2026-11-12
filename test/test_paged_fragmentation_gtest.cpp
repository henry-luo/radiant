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
    void expect_text_page(const char* id, uint32_t page) {
        DomElement* owner = dom_find_element_by_id(session.document->root, id);
        ASSERT_NE(owner, nullptr);
        ASSERT_TRUE(owner->first_child && owner->first_child->is_text());
        size_t count = 0;
        visit_occurrences(owner->first_child, [&](const LayoutViewNode* node) {
            if (node->role != VIEW_FRAGMENT_BODY || !node->text_length) return;
            EXPECT_EQ(page_of(node), page); count++;
        });
        EXPECT_EQ(count, 1u);
    }
    void expect_failed_admission_keeps_pixels(const PagedLayoutOptions& options, uint32_t page) {
        ViewEnvironment environment = session.paged_view->model->environment;
        ViewTree* limited = view_tree_secondary_create(session.document, &environment);
        ASSERT_NE(limited, nullptr);
        ImageSurface* before = render_secondary_page_snapshot(session.paged_view, page);
        EXPECT_NE(before, nullptr);
        PagedLayoutDiagnostic diagnostic = {};
        EXPECT_EQ(layout_secondary_view(limited, &options, &diagnostic), TYPESET_BUDGET_EXHAUSTED);
        EXPECT_FALSE(limited->model->committed);
        ImageSurface* after = render_secondary_page_snapshot(session.paged_view, page);
        EXPECT_NE(after, nullptr);
        if (before && after) {
            EXPECT_EQ(before->width, after->width); EXPECT_EQ(before->height, after->height); EXPECT_EQ(before->pitch, after->pitch);
            if (before->width == after->width && before->height == after->height && before->pitch == after->pitch)
                EXPECT_EQ(memcmp(before->pixels, after->pixels, (size_t)before->pitch * before->height), 0);
        }
        image_surface_destroy(before); image_surface_destroy(after);
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
    bool compose_variable_rows(const char* extra = "", const char* body =
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div></div><div id='tail' style='height:20px'>Tail</div>") {
        char css[1200];
        snprintf(css, sizeof(css), "@page:left{size:300px 100px;margin:0}#rows{display:%s;flex-wrap:wrap;grid-template-columns:minmax(0,1fr) minmax(0,1fr);grid-auto-rows:60px;align-items:start}#rows>div{height:60px;min-width:0;flex:0 0 50%%;box-sizing:border-box}%s", GetParam(), extra);
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
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 2);
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

TEST_F(PagedFragmentationTest, EndRelativeDestinationsReserveBeforeTheirLateAnchor) {
    ASSERT_TRUE(compose("-3", "top", "", "<article id='first'>A</article><article id='anchor'><figure id='figure'></figure>Anchor</article><article>C</article>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    occurrence_page("first", VIEW_FRAGMENT_BODY, &count, &rect); EXPECT_FLOAT_EQ(rect.y, 20.0f);
    EXPECT_EQ(occurrence_page("anchor", VIEW_FRAGMENT_BODY, &count), 2u);
    expect_text_coverage("first");
}

TEST_F(PagedFragmentationTest, PercentageContainingChainsRemainOutsideFixedWidthContinuation) {
    EXPECT_FALSE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div style='width:50%'><article>A</article><article style='page:wide'>Wide</article></div>"));
}

TEST_F(PagedFragmentationTest, IndependentFormattingContextsDoNotReuseTheOrdinaryWidthChain) {
    EXPECT_FALSE(compose("0", "top", "@page wide{size:300px 100px;margin:0}",
        "<div style='display:flow-root;width:150px'><article>A</article><article style='page:wide'>Wide</article></div>"));
}

TEST_F(PagedFragmentationTest, BalancedColumnsReserveATopFigureBeforeTheirBody) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:20px;break-inside:avoid'><div id='figure' style='float:top;float-reference:column;width:60px;height:20px'></div></div><div id='b' style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div><div id='d' style='height:20px;break-inside:avoid'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 0.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_FLOAT_EQ(rect.x, 0.0f); EXPECT_FLOAT_EQ(rect.y, 40.0f);
    EXPECT_EQ(occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 20.0f);
}

TEST_F(PagedFragmentationTest, BalancedColumnsPlaceABottomFigureAtTheBalancedEdge) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:20px;break-inside:avoid'><div id='figure' style='float:bottom;float-reference:column;width:60px;height:20px'></div></div><div style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div><div id='d' style='height:20px;break-inside:avoid'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 40.0f); EXPECT_FLOAT_EQ(rect.height, 20.0f);
    EXPECT_EQ(occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_FLOAT_EQ(rect.x, 100.0f);
}

TEST_F(PagedFragmentationTest, BalancedColumnsCountConcurrentAuxiliaryReservationsOnce) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:20px;break-inside:avoid;font:10px/20px monospace'>A<div id='figure' style='float:top;float-reference:column;width:60px;height:20px'></div><div id='second' style='float:bottom;float-reference:column;width:60px;height:20px'></div></div><div style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    size_t count; RdtLogicalRect rect = {};
    occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    occurrence_page("second", VIEW_FRAGMENT_FLOAT, &count, &rect); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 40.0f);
}

TEST_F(PagedFragmentationTest, BalancedColumnsKeepTheirFootnoteAndEveryTextByte) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:20px;break-inside:avoid;font:10px/20px monospace'>Call<span id='note' style='float:footnote;footnote-policy:line;display:block;width:100px;height:20px'>Note</span></div><div style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div><div id='d' style='height:20px;break-inside:avoid'></div>"));
    ASSERT_EQ(session.paged_view->model->page_count, 1u);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("note", VIEW_FRAGMENT_NOTE, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_LE(rect.y + rect.height, 60.01f);
    EXPECT_FLOAT_EQ(session.paged_view->model->pages.get()[0]->style->balanced_height, 60.0f);
    DomElement* note = dom_find_element_by_id(session.document->root, "note");
    ASSERT_NE(note, nullptr);
    DomNode* text = note->first_child;
    while (text && !text->is_text()) text = text->next_sibling;
    expect_text_source_coverage(text);
}

TEST_F(PagedFragmentationTest, ABackwardBottomFigureReservesTheDestinationEdge) {
    ASSERT_TRUE(compose("-3", "bottom", "", "<article id='first'>A</article><article><figure id='figure'></figure>Anchor</article><article>C</article>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 80.0f);
    occurrence_page("first", VIEW_FRAGMENT_BODY, &count, &rect); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, BackwardReservationsRefitEarlierBodyLinesWithoutOverlap) {
    ASSERT_TRUE(compose("-3", "top", "p{margin:0;font:10px/20px monospace;widows:1;orphans:1}",
        "<p><span id='l1'>One</span><br/><span id='l2'>Two</span><br/><span id='l3'>Three</span><br/><span id='l4'>Four</span><br/><span id='l5'>Five</span><br/><span id='l6'>Six</span></p><article id='anchor' style='break-before:page'><figure id='figure'></figure>Anchor</article>"));
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.height, 20.0f);
    expect_text_page("l4", 1);
    expect_text_page("l5", 2);
    EXPECT_EQ(occurrence_page("anchor", VIEW_FRAGMENT_BODY, &count), 3u);
    const char* ids[] = {"l1", "l2", "l3", "l4", "l5", "l6"};
    for (const char* id : ids) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, BackwardFiguresUseDestinationSizingAndSourceOrder) {
    ASSERT_TRUE(compose("-3", "top", "@page:first{size:200px 200px}figure{height:25%}",
        "<article>A</article><article><figure id='figure'></figure><figure id='second'></figure>Anchor</article><article>C</article>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("figure", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.height, 50.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("second", VIEW_FRAGMENT_FLOAT, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.height, 50.0f); EXPECT_FLOAT_EQ(rect.y, 50.0f);
}

TEST_F(PagedFragmentationTest, AuxiliaryBalancePassFailureKeepsTheRetainedPixels) {
    ASSERT_TRUE(compose_columns("balance", "<div style='height:20px;break-inside:avoid'>A<div id='figure' style='float:top;float-reference:column;width:60px;height:20px;background:lime'></div></div><div style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div><div style='height:20px;break-inside:avoid'></div>"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_reference_passes = 1;
    expect_failed_admission_keeps_pixels(options, 1);
}

TEST_F(PagedFragmentationTest, BackwardReservationReplayHonorsThePassAndPageBudgets) {
    ASSERT_TRUE(compose("-3", "top", "", "<article id='first'>A</article><article><figure id='figure'></figure>Anchor</article><article>C</article>"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_reference_passes = 2;
    expect_failed_admission_keeps_pixels(options, 1);
    options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 1);
}

TEST_P(PagedRowTest, OversizedItemsFitTheirNestedBlockContentWithoutMovingTheirPeers) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:auto;align-items:start}#rows>div{height:auto}#rows p{margin:0;height:30px;break-inside:avoid;font:10px/20px monospace}",
        "<div id='rows'><div id='a'><p id='n1'>One</p><p id='n2'>Two</p><p id='n3'>Three</p><p id='n4'>Four</p><p id='n5'>Five</p></div><div id='b' style='height:30px'>Peer</div><div id='c' style='height:20px'>After</div><div style='height:20px'>After</div></div><div id='tail' style='height:20px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("n4", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("n5", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 30.0f);
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_FLOAT_EQ(rect.y, 60.0f);
    const char* ids[] = {"n1", "n2", "n3", "n4", "n5", "b", "c", "tail"};
    for (const char* id : ids) expect_text_coverage(id);
}

TEST_P(PagedRowTest, NestedFormattingRowsShareInternalContinuationAndFollowingGrowth) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:auto;align-items:start}#rows>div{height:auto}.inner{display:grid;grid-template-columns:100px;grid-auto-rows:60px}.inner>div{height:60px}",
        "<div id='rows'><div class='inner'><div id='n1'>One</div><div id='n2'>Two</div><div id='n3'>Three</div></div><div id='b' style='height:20px'>Peer</div><div id='c' style='height:20px'>After</div><div style='height:20px'>After</div></div><div id='tail' style='height:20px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("n2", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("n3", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_FLOAT_EQ(rect.y, 60.0f);
    const char* ids[] = {"n1", "n2", "n3", "b", "c", "tail"};
    for (const char* id : ids) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, ColumnFlexItemsBreakInTheirResolvedBlockOrder) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-direction:column;align-items:start}#rows>div{height:60px;flex:none}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div></div><div id='tail' style='height:20px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 1u); EXPECT_EQ(count, 1u);
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_FLOAT_EQ(rect.y, 60.0f);
}

TEST_P(PagedRowTest, OversizedItemTextRetainsItsWidowsAndAuthoredIntervals) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:auto;align-items:start}#rows>div{height:auto}#rows p{margin:0;font:10px/30px monospace;widows:2;orphans:2}",
        "<div id='rows'><div><p><span id='n1'>One</span><br/><span id='n2'>Two</span><br/><span id='n3'>Three</span><br/><span id='n4'>Four</span><br/><span id='n5'>Five</span></p></div><div id='b' style='height:20px'>Peer</div></div><div id='tail' style='height:20px'>Tail</div>"));
    expect_text_page("n3", 1); expect_text_page("n4", 2); expect_text_page("n5", 2);
    const char* ids[] = {"n1", "n2", "n3", "n4", "n5", "b", "tail"};
    for (const char* id : ids) expect_text_coverage(id);
}

TEST_P(PagedRowTest, AnExactRowEndTrimsTheGapBeforeTheContinuation) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:100px;row-gap:10px}#rows>div{height:100px}"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_P(PagedRowTest, NestedRowBudgetFailurePreservesThePublishedPixels) {
    ASSERT_TRUE(compose_rows("#rows{grid-auto-rows:auto;align-items:start}#rows>div{height:auto}.inner{display:grid;grid-template-columns:100px;grid-auto-rows:60px}.inner>div{height:60px;background:lime}",
        "<div id='rows'><div class='inner'><div>One</div><div>Two</div><div>Three</div></div><div style='height:20px'>Peer</div></div>"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 1);
}

TEST_F(PagedFragmentationTest, ReversedColumnFlexKeepsTheResolvedPhysicalOrder) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-direction:column-reverse;align-items:start}#rows>div{height:60px;flex:none}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div></div><div id='tail' style='height:20px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 1u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, BackwardFiguresShareOneReservationDiscoveryPass) {
    char body[1400];
    strcpy(body, "<article>A</article><article id='anchor'>Anchor<figure id='figure'></figure>");
    for (size_t i = 1; i < 20; i++) strcat(body, "<figure></figure>");
    strcat(body, "</article><article>C</article>");
    ASSERT_TRUE(compose("-3", "top", "figure{height:2px}", body));
    ASSERT_EQ(session.paged_view->model->page_count, 3u);
    DomElement* anchor = dom_find_element_by_id(session.document->root, "anchor"); ASSERT_NE(anchor, nullptr);
    size_t count = 0;
    for (DomNode* child = anchor->first_child; child; child = child->next_sibling) {
        if (!child->is_element() || child->as_element()->tag() != MARKUP_NAME_FIGURE) continue;
        size_t occurrences = 0;
        visit_occurrences(child, [&](const LayoutViewNode* node) {
            if (node->role != VIEW_FRAGMENT_FLOAT || !node->paint_box) return;
            EXPECT_EQ(page_of(node), 1u); EXPECT_FLOAT_EQ(node->rect.y, (float)count * 2.0f); occurrences++;
        });
        EXPECT_EQ(occurrences, 1u); count++;
    }
    EXPECT_EQ(count, 20u);
}


TEST_P(PagedRowTest, FlexibleTracksUseEachDestinationPageWidth) {
    ASSERT_TRUE(compose_variable_rows());
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.width, 150.0f);
    EXPECT_EQ(occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.x, 150.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.width, 100.0f);
    for (const char* id : {"a", "b", "c", "d", "e", "f", "tail"}) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, WrappedColumnFlexLanesContinueIndependently) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-flow:column wrap;height:180px;align-content:start}#rows>div{height:60px;width:100px;flex:none}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div></div><div id='tail' style='height:20px'>Tail</div>"));
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    size_t count; RdtLogicalRect rect = {};
    for (const char* id : {"b", "e"}) {
        EXPECT_EQ(occurrence_page(id, VIEW_FRAGMENT_BODY, &count, &rect), 2u);
        EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    }
    EXPECT_EQ(occurrence_page("f", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, 100.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect); EXPECT_FLOAT_EQ(rect.y, 60.0f);
    for (const char* id : {"a", "b", "c", "d", "e", "f"}) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, VerticalFlexUsesItsPhysicalPaginationAxis) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;writing-mode:vertical-lr;flex-direction:row;align-items:start}#rows>div{height:60px;width:30px;flex:none}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div></div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    for (const char* id : {"a", "b", "c"}) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, VerticalGridBreaksBetweenPhysicalColumns) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:grid;writing-mode:vertical-lr;grid-template-columns:60px 60px 60px;grid-template-rows:30px 30px}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div></div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    for (const char* id : {"a", "b", "c", "d", "e", "f"}) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, OrthogonalLineBoxesMoveWithTheirFlowRoot) {
    ASSERT_TRUE(compose("0", "top", "#vertical{writing-mode:vertical-rl;height:80px;width:60px;font:10px/20px monospace}",
        "<div style='height:40px'></div><div id='vertical'>Orthogonal words</div><div id='tail' style='height:10px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("vertical", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 0.0f); EXPECT_FLOAT_EQ(rect.height, 80.0f);
    expect_text_coverage("vertical"); expect_text_coverage("tail");
}


TEST_P(PagedRowTest, DestinationTracksRewrapTextAndAdvanceFollowingRows) {
    ASSERT_TRUE(compose_variable_rows("@page:left{size:100px 100px;margin:0}#rows{grid-auto-rows:auto}#rows>div{height:auto;font:10px/20px monospace}#rows>.first{height:80px}",
        "<div id='rows'><div class='first'>A</div><div class='first'>B</div><div id='c'>aaaa aaaa aaaa aaaa aaaa</div><div id='d'>bbbb bbbb bbbb bbbb bbbb</div><div id='e'>End</div><div>End</div></div><div id='tail' style='height:20px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.width, 50.0f); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    for (const char* id : {"c", "d", "e", "tail"}) expect_text_coverage(id);
    DomElement* text = dom_find_element_by_id(session.document->root, "c");
    size_t lines = 0;
    visit_occurrences(text->first_child, [&](const LayoutViewNode* node) {
        if (!node->text_length) return;
        EXPECT_LE(node->rect.x + node->rect.width, 50.01f); EXPECT_EQ(page_of(node), 2u); lines++;
    });
    EXPECT_EQ(lines, 5u);
}

TEST_P(PagedRowTest, AbsoluteWidthTracksKeepTheirMeasureOnWiderPages) {
    ASSERT_TRUE(compose_variable_rows("#rows{width:160px}"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.width, 80.0f);
    occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect); EXPECT_FLOAT_EQ(rect.x, 80.0f);
}

TEST_P(PagedRowTest, TrackReplayBudgetFailureKeepsPublishedPixels) {
    ASSERT_TRUE(compose_variable_rows("#rows>div{background:lime}"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 2);
    options = paged_layout_options_default(); options.max_reference_passes = 1;
    expect_failed_admission_keeps_pixels(options, 2);
}

TEST_F(PagedFragmentationTest, ChangingFlexRowMembershipContinuesBeforePublication) {
    EXPECT_TRUE(compose("0", "top", "@page:left{size:300px 100px;margin:0}#rows{display:flex;flex-wrap:wrap}#rows>div{width:100px;height:60px;flex:none}",
        "<div id='rows'><div>A</div><div>B</div><div>C</div><div>D</div><div>E</div><div>F</div></div>"));
}

TEST_F(PagedFragmentationTest, AForcedColumnLaneBreakDoesNotMoveItsParallelPeer) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-flow:column wrap;height:160px;align-content:start}#rows>div{height:50px;width:100px;flex:none}#b{break-before:page}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div></div><div id='tail' style='height:10px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 1u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.y, 50.0f);
    EXPECT_EQ(occurrence_page("f", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    for (const char* id : {"a", "b", "c", "d", "e", "f", "tail"}) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, WrappedReversedColumnLanesKeepTheirPhysicalOrder) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-flow:column-reverse wrap-reverse;height:180px;align-content:start}#rows>div{height:60px;width:100px;flex:none}",
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div></div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 1u); EXPECT_EQ(count, 1u);
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_EQ(count, 1u);
    EXPECT_FLOAT_EQ(rect.y, 0.0f);
    for (const char* id : {"a", "b", "c", "d", "e", "f"}) expect_text_coverage(id);
}

TEST_F(PagedFragmentationTest, WrappedColumnBudgetFailureKeepsPublishedPixels) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-flow:column wrap;height:180px;align-content:start}#rows>div{height:60px;width:100px;flex:none;background:lime}",
        "<div id='rows'><div>A</div><div>B</div><div>C</div><div>D</div><div>E</div><div>F</div></div>"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 2);
}


TEST_P(PagedRowTest, TrackGeometryReplayRetainsSourceOrderedGeneratedCounters) {
    ASSERT_TRUE(compose_variable_rows("#rows{counter-reset:part}#rows>div{counter-increment:part}#rows>div::before{content:counter(part)}"));
    const char* ids[] = {"a", "b", "c", "d", "e", "f"};
    for (size_t i = 0; i < 6; i++) {
        DomElement* element = dom_find_element_by_id(session.document->root, ids[i]);
        ASSERT_TRUE(element && element->pseudo && element->pseudo->before);
        DomNode* text = element->pseudo->before->first_child;
        ASSERT_TRUE(text && text->is_text());
        char expected[2] = {(char)('1' + i), 0};
        EXPECT_STREQ(text->as_text()->text, expected);
        // generated glyphs publish against the originating element, not the synthetic text node.
        size_t published = 0;
        visit_occurrences(element, [&](const LayoutViewNode* node) {
            if (!node->generated || !node->glyph_run) return;
            EXPECT_EQ(node->glyph_run->text_len, 1);
            EXPECT_EQ(node->glyph_run->text[0], expected[0]);
            EXPECT_EQ(page_of(node), i / 2 + 1); published++;
        });
        EXPECT_EQ(published, 1u);
    }
}

TEST_P(PagedRowTest, ManyRowsContinueAcrossAlternatingWidthPages) {
    char html[24000];
    snprintf(html, sizeof(html), "<!doctype html><style>@page{size:200px 100px;margin:0}@page:left{size:300px 100px;margin:0}html,body{margin:0}#rows{display:%s;flex-wrap:wrap;grid-template-columns:minmax(0,1fr) minmax(0,1fr);grid-auto-rows:60px}#rows>div{height:60px;min-width:0;flex:0 0 50%%}</style><div id='rows'>", GetParam());
    for (size_t i = 0; i < 240; i++) {
        char cell[96]; snprintf(cell, sizeof(cell), "<div id='cell-%zu'>Item%zu</div>", i, i); strcat(html, cell);
    }
    strcat(html, "</div><div id='tail' style='height:20px'>Tail</div>");
    ASSERT_TRUE(compose_source(html, "html"));
    EXPECT_EQ(session.paged_view->model->page_count, 120u);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("cell-120", VIEW_FRAGMENT_BODY, &count, &rect), 61u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.width, 100.0f);
    EXPECT_EQ(occurrence_page("cell-122", VIEW_FRAGMENT_BODY, &count, &rect), 62u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.width, 150.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 120u);
    EXPECT_FLOAT_EQ(rect.y, 60.0f);
    for (size_t i = 0; i < 240; i++) {
        char id[32]; snprintf(id, sizeof(id), "cell-%zu", i); expect_text_coverage(id);
    }
}


TEST_F(PagedFragmentationTest, WrappedColumnFirstItemBreakPropagatesToTheContainer) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-flow:column wrap;height:180px;align-content:start}#rows>div{height:60px;width:100px;flex:none}#a{break-before:page}",
        "<div style='height:20px'></div><div id='rows'><div id='a'>A</div><div>B</div><div>C</div><div id='d'>D</div><div>E</div><div>F</div></div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("a", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
    EXPECT_EQ(occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}

TEST_F(PagedFragmentationTest, WrappedColumnLastItemBreakPropagatesToFollowingFlow) {
    ASSERT_TRUE(compose("0", "top", "#rows{display:flex;flex-flow:column wrap;height:180px;align-content:start}#rows>div{height:60px;width:100px;flex:none}#f{break-after:page}",
        "<div id='rows'><div>A</div><div>B</div><div>C</div><div>D</div><div>E</div><div id='f'>F</div></div><div id='tail' style='height:10px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 4u); EXPECT_FLOAT_EQ(rect.y, 0.0f);
}


TEST_P(PagedRowTest, RemainingItemsRepackWhenDestinationTrackTopologyChanges) {
    char css[640];
    snprintf(css, sizeof(css), "@page:left{size:300px 100px;margin:0}#rows{display:%s;flex-wrap:wrap;grid-template-columns:repeat(auto-fill,100px);grid-auto-rows:60px;align-items:start}#rows>div{width:100px;height:60px;flex:none}", GetParam());
    ASSERT_TRUE(compose("0", "top", css,
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div><div id='g'>G</div><div id='h'>H</div></div><div id='tail' style='height:20px'>Tail</div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(session.paged_view->model->page_count, 4u);
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_EQ(occurrence_page("e", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_FLOAT_EQ(rect.x, 200.0f);
    EXPECT_EQ(occurrence_page("f", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_EQ(occurrence_page("h", VIEW_FRAGMENT_BODY, &count, &rect), 4u); EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 4u); EXPECT_FLOAT_EQ(rect.y, 60.0f);
    for (const char* id : {"a","b","c","d","e","f","g","h","tail"}) expect_text_coverage(id);
}


TEST_P(PagedRowTest, SplitItemTextUsesTheWidthOfEveryContinuationTrack) {
    ASSERT_TRUE(compose_variable_rows("@page:left{size:100px 100px;margin:0}#rows{grid-auto-rows:auto}#rows>div{height:auto}#rows p{margin:0;font:10px/20px monospace;orphans:2;widows:2}",
        "<div id='rows'><div id='a'><p id='words'>aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa</p></div><div id='b' style='height:20px'>Peer</div></div><div id='tail' style='height:20px'>Tail</div>"));
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    expect_text_coverage("words"); expect_text_coverage("b"); expect_text_coverage("tail");
    DomElement* words = dom_find_element_by_id(session.document->root, "words");
    size_t narrow = 0;
    visit_occurrences(words->first_child, [&](const LayoutViewNode* node) {
        if (page_of(node) != 2 || !node->text_length) return;
        EXPECT_LE(node->rect.x + node->rect.width, 50.01f); narrow++;
    });
    EXPECT_EQ(narrow, 5u);
    visit_occurrences(words, [&](const LayoutViewNode* node) {
        if (!node->paint_box || node->role != VIEW_FRAGMENT_BODY) return;
        EXPECT_FLOAT_EQ(node->rect.width, page_of(node) == 2 ? 50.0f : 100.0f);
    });
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_FLOAT_EQ(rect.y, 40.0f);
}


class PagedVerticalRootTest : public PagedFragmentationTest, public ::testing::WithParamInterface<const char*> {
protected:
    bool compose_vertical(const char* css, const char* body) {
        char html[4096];
        snprintf(html, sizeof(html), "<!doctype html><html><head><style>@page{size:200px 100px;margin:0}html,body{margin:0}body{writing-mode:%s;height:100px}%s</style></head><body>%s</body></html>", GetParam(), css, body);
        return compose_source(html, "html");
    }
};

TEST_P(PagedVerticalRootTest, PrincipalBlockAxisAdvancesOntoPhysicalSheets) {
    ASSERT_TRUE(compose_vertical("body>div{width:80px;height:100px;break-inside:avoid}#tail{width:20px}",
        "<div id='a' style='background:blue'></div><div id='b' style='background:red'></div><div id='c' style='background:lime'></div><div id='tail'></div>"));
    EXPECT_EQ(session.paged_view->model->page_count, 2u);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_EQ(count, 1u); EXPECT_FLOAT_EQ(rect.x, GetParam()[9] == 'r' ? 120.0f : 0.0f);
    EXPECT_FLOAT_EQ(rect.y, 0.0f); EXPECT_FLOAT_EQ(rect.width, 80.0f); EXPECT_FLOAT_EQ(rect.height, 100.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.x, GetParam()[9] == 'r' ? 100.0f : 80.0f);
}

TEST_P(PagedVerticalRootTest, VerticalLineBreaksKeepWidowsAndEveryAuthoredByte) {
    ASSERT_TRUE(compose_vertical("p{margin:0;font:10px/30px monospace;orphans:2;widows:2}",
        "<p id='words'><span id='n1'>A</span><br/><span id='n2'>B</span><br/><span id='n3'>C</span><br/><span id='n4'>D</span><br/><span id='n5'>E</span><br/><span id='n6'>F</span><br/><span id='n7'>G</span></p>"));
    EXPECT_EQ(session.paged_view->model->page_count, 2u);
    expect_text_page("n5", 1); expect_text_page("n6", 2); expect_text_page("n7", 2);
    for (const char* id : {"n1","n2","n3","n4","n5","n6","n7"}) expect_text_coverage(id);
}

INSTANTIATE_TEST_SUITE_P(PrincipalModes, PagedVerticalRootTest, ::testing::Values("vertical-lr", "vertical-rl"));


TEST_P(PagedRowTest, ShrinkingTopologyRetainsSourceOrderAndGeneratedCounters) {
    char css[768];
    snprintf(css, sizeof(css), "@page{size:300px 100px;margin:0}@page:left{size:200px 100px;margin:0}#rows{display:%s;flex-wrap:wrap;grid-template-columns:repeat(auto-fill,100px);grid-auto-rows:60px;counter-reset:part}#rows>div{width:100px;height:60px;flex:none;counter-increment:part}#rows>div::before{content:counter(part)}", GetParam());
    ASSERT_TRUE(compose("0", "top", css,
        "<div id='rows'><div id='a'>A</div><div id='b'>B</div><div id='c'>C</div><div id='d'>D</div><div id='e'>E</div><div id='f'>F</div><div id='g'>G</div><div id='h'>H</div></div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("d", VIEW_FRAGMENT_BODY, &count, &rect), 2u); EXPECT_FLOAT_EQ(rect.x, 0.0f);
    EXPECT_EQ(occurrence_page("f", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_FLOAT_EQ(rect.x, 0.0f);
    const char* ids[] = {"a","b","c","d","e","f","g","h"};
    for (size_t i = 0; i < 8; i++) {
        DomElement* item = dom_find_element_by_id(session.document->root, ids[i]); ASSERT_NE(item, nullptr);
        size_t generated = 0;
        visit_occurrences(item, [&](const LayoutViewNode* node) {
            if (!node->generated || !node->glyph_run) return;
            EXPECT_EQ(node->glyph_run->text_len, 1); EXPECT_EQ(node->glyph_run->text[0], '1' + i); generated++;
        });
        EXPECT_EQ(generated, 1u);
        DomNode* text = item->first_child;
        while (text && !text->is_text()) text = text->next_sibling;
        expect_text_source_coverage(text);
    }
}

TEST_P(PagedRowTest, ParallelSplitItemsReflowIndependentlyAtTheirPhysicalTracks) {
    const char* words = "aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa aaaa";
    char body[1024];
    snprintf(body, sizeof(body), "<div id='rows'><div><p id='left'>%s</p></div><div><p id='right'>%s</p></div></div><div id='tail' style='height:20px'>Tail</div>", words, words);
    ASSERT_TRUE(compose_variable_rows("@page:left{size:100px 100px;margin:0}#rows{grid-auto-rows:auto}#rows>div{height:auto}#rows p{margin:0;font:10px/20px monospace;orphans:2;widows:2}",body));
    EXPECT_EQ(session.paged_view->model->page_count, 3u);
    DomElement* right = dom_find_element_by_id(session.document->root, "right"); ASSERT_NE(right, nullptr);
    size_t lines = 0;
    visit_occurrences(right->first_child, [&](const LayoutViewNode* node) {
        if (page_of(node) != 2 || !node->text_length) return;
        EXPECT_GE(node->rect.x, 50.0f); EXPECT_LE(node->rect.x + node->rect.width, 100.01f); lines++;
    });
    EXPECT_EQ(lines, 5u); expect_text_coverage("left"); expect_text_coverage("right");
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u); EXPECT_FLOAT_EQ(rect.y, 40.0f);
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 2);
    options = paged_layout_options_default(); options.max_reference_passes = 1;
    expect_failed_admission_keeps_pixels(options, 2);
}

TEST_P(PagedRowTest, TopologyBudgetFailureKeepsThePreviousEdition) {
    char css[512];
    snprintf(css, sizeof(css), "@page:left{size:300px 100px;margin:0}#rows{display:%s;flex-wrap:wrap;grid-template-columns:repeat(auto-fill,100px);grid-auto-rows:60px}#rows>div{width:100px;height:60px;flex:none;background:lime}", GetParam());
    ASSERT_TRUE(compose("0","top",css,"<div id='rows'><div>A</div><div>B</div><div>C</div><div>D</div><div>E</div><div>F</div><div>G</div><div>H</div></div>"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 2;
    expect_failed_admission_keeps_pixels(options, 1);
}

TEST_P(PagedVerticalRootTest, ForcedVerticalBreaksApplyAtTheBlockBoundary) {
    ASSERT_TRUE(compose_vertical("body>div{width:80px;height:100px}#b{break-before:page}#c{break-after:page}#tail{width:20px}",
        "<div id='a'></div><div id='b'></div><div id='c'></div><div id='tail'></div>"));
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("b", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.x, GetParam()[9] == 'r' ? 120.0f : 0.0f);
    EXPECT_EQ(occurrence_page("tail", VIEW_FRAGMENT_BODY, &count, &rect), 3u);
    EXPECT_FLOAT_EQ(rect.x, GetParam()[9] == 'r' ? 180.0f : 0.0f);
}

TEST_P(PagedVerticalRootTest, VariableSheetWidthsSetThePrincipalBlockCapacity) {
    ASSERT_TRUE(compose_vertical("@page:left{size:300px 100px;margin:0}body>div{width:100px;height:100px}",
        "<div>A</div><div>B</div><div id='c'>C</div><div>D</div><div id='last'>E</div>"));
    EXPECT_EQ(session.paged_view->model->page_count, 2u);
    size_t count; RdtLogicalRect rect = {};
    EXPECT_EQ(occurrence_page("c", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.x, GetParam()[9] == 'r' ? 200.0f : 0.0f);
    EXPECT_EQ(occurrence_page("last", VIEW_FRAGMENT_BODY, &count, &rect), 2u);
    EXPECT_FLOAT_EQ(rect.x, GetParam()[9] == 'r' ? 0.0f : 200.0f);
    expect_text_coverage("c"); expect_text_coverage("last");
}

TEST_P(PagedVerticalRootTest, FailedVerticalAdmissionPreservesTheRetainedPixels) {
    ASSERT_TRUE(compose_vertical("body>div{width:80px;height:100px;break-inside:avoid;background:lime}",
        "<div></div><div></div><div></div>"));
    PagedLayoutOptions options = paged_layout_options_default(); options.max_pages = 1;
    expect_failed_admission_keeps_pixels(options, 1);
}


TEST_P(PagedVerticalRootTest, ContinuousVerticalFlowKeepsItsUnboundedPhysicalFrame) {
    ASSERT_TRUE(compose_vertical("p{margin:0;font:10px/30px monospace}", "<p>ABC</p>"));
    ViewEnvironment environment = session.paged_view->model->environment;
    environment.presentation = VIEW_PRESENTATION_CONTINUOUS;
    ViewTree* continuous = view_tree_secondary_create(session.document, &environment);
    ASSERT_NE(continuous, nullptr);
    PagedLayoutOptions options = paged_layout_options_default(); PagedLayoutDiagnostic diagnostic = {};
    ASSERT_EQ(layout_secondary_view(continuous, &options, &diagnostic), TYPESET_OK) << diagnostic.reason;
    EXPECT_TRUE(continuous->model->committed); EXPECT_EQ(continuous->model->page_count, 0u);
    size_t bytes = 0;
    for (size_t i = 0; i < continuous->model->node_id_count; i++) {
        const LayoutViewNode* node = continuous->model->nodes[i];
        if (node && node->glyph_run) bytes += (size_t)node->glyph_run->text_len;
    }
    EXPECT_EQ(bytes, 3u);
    view_tree_secondary_release(session.document, continuous);
}
