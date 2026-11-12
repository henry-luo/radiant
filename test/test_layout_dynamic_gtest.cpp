#include <gtest/gtest.h>
#include "radiant/radiant.hpp"
#include "radiant/layout.hpp"
#include "radiant/tagged.hpp"
#include "radiant/view.hpp"
#include "radiant/render.hpp"
#include "radiant/event_hit_test.hpp"
#include "radiant/event.hpp"
#include "radiant/network_integration.h"
#include "lambda/input/input.hpp"
#include "lambda/network/enhanced_file_cache.h"
#include "lambda/network/cookie_jar.h"
#include "lambda/input/css/css_engine.hpp"
#include "lambda/io/image_io.h"
#include "lib/file.h"
#include "lib/mem.h"
#include "lib/strbuf.h"
#include "lib/host_paths.h"
#include "lib/url.h"

TEST(LayoutCacheTest, ZeroDimensionsRemainDistinctFromAutoCacheKeys) {
    ViewBlock block = {};
    BlockProp parent_blk = {};
    block.blk = lam::view_prop(&parent_blk);
    AvailableSpace space = AvailableSpace::make_indefinite();
    radiant::LayoutCache cache = {};
    radiant::layout_cache_init(&cache);
    for (bool horizontal : {true, false}) {
        SCOPED_TRACE(horizontal ? "width" : "height");
        parent_blk.given_width = horizontal ? 0.0f : -1.0f;
        parent_blk.given_height = horizontal ? -1.0f : 0.0f;
        radiant::KnownDimensions zero = radiant::layout_known_dimensions_from_block(&block);
        EXPECT_EQ(zero.has_width, horizontal);
        EXPECT_EQ(zero.has_height, !horizontal);
        radiant::layout_cache_store(&cache, zero, space,
            radiant::RunMode::ComputeSize, radiant::size_f(0.0f, 0.0f));
        parent_blk.given_width = parent_blk.given_height = -1.0f;
        radiant::KnownDimensions automatic = radiant::layout_known_dimensions_from_block(&block);
        EXPECT_FALSE(automatic.has_width);
        EXPECT_FALSE(automatic.has_height);
        radiant::SizeF result = {};
        EXPECT_FALSE(radiant::layout_cache_get(&cache, automatic, space,
            radiant::RunMode::ComputeSize, &result));
        EXPECT_TRUE(radiant::layout_cache_get(&cache, zero, space,
            radiant::RunMode::ComputeSize, &result));
        radiant::layout_cache_init(&cache);
    }
}

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

    void load_nested_flex_toolbar(const char* outer_wrap, const char* inner_wrap) {
        StrBuf* html = strbuf_create("<!doctype html><style>"
            "html,body{margin:0}"
            "#app{width:1400px;height:960px;display:flex;flex-direction:column}"
            "#bar{display:flex;align-items:center;gap:10px;padding:8px;flex-wrap:");
        strbuf_append_str(html, outer_wrap);
        strbuf_append_str(html, "}.group{display:flex;align-items:center;gap:4px}"
            ".wrap{max-width:420px;flex-wrap:");
        strbuf_append_str(html, inner_wrap);
        strbuf_append_str(html, "}.group span{display:inline-block;width:90px;height:24px}"
            "#stage{flex:1;overflow:hidden}#status{height:28px}"
            "</style><div id='app'><div id='bar'>");
        const int counts[] = {4, 7, 3, 4};
        for (int group = 0; group < 4; group++) {
            strbuf_append_str(html, group == 1 ? "<div class='group wrap'>" : "<div class='group'>");
            for (int child = 0; child < counts[group]; child++)
                strbuf_append_str(html, "<span></span>");
            strbuf_append_str(html, "</div>");
        }
        strbuf_append_str(html, "</div><main id='stage'></main><footer id='status'></footer></div>");
        load(html->str);
        strbuf_free(html);
    }

    void check_flex_toolbar(float toolbar_height) {
        DomElement* toolbar = element("bar");
        DomElement* stage = element("stage");
        DomElement* status = element("status");
        ASSERT_NE(toolbar, nullptr);
        ASSERT_NE(stage, nullptr);
        ASSERT_NE(status, nullptr);
        EXPECT_FLOAT_EQ(toolbar->height, toolbar_height);
        EXPECT_FLOAT_EQ(stage->y, toolbar_height);
        EXPECT_FLOAT_EQ(stage->height, 960.0f - toolbar_height - 28.0f);
        EXPECT_FLOAT_EQ(status->y, 932.0f);
        EXPECT_FLOAT_EQ(status->height, 28.0f);
    }
};

TEST_F(DynamicLayoutTest, StylesheetRelKeywordsAreCaseInsensitiveHtmlTokens) {
    EXPECT_TRUE(radiant_html_has_token(" preload\tSTYLESHEET\r\n", "stylesheet"));
    EXPECT_TRUE(radiant_html_has_token("stylesheet\fpreload", "stylesheet"));
    EXPECT_FALSE(radiant_html_has_token("notstylesheet", "stylesheet"));
    EXPECT_FALSE(radiant_html_has_token("stylesheet\vpreload", "stylesheet"));
    EXPECT_FALSE(radiant_html_has_token(nullptr, "stylesheet"));
    ASSERT_EQ(write_text_file_atomic("temp/token_stylesheet.css",
        "#sample{width:600px}"), 0);
    ASSERT_EQ(write_text_file_atomic("temp/token_not_stylesheet.css",
        "#sample{width:8px}"), 0);
    load("<!doctype html><style>#sample{width:40px}</style>"
         "<link rel='preload\tSTYLESHEET\n' href='token_stylesheet.css'>"
         "<link rel='notstylesheet' href='token_not_stylesheet.css'>"
         "<div id='sample'>Visible content</div>");
    ASSERT_NE(element("sample"), nullptr);
    EXPECT_FLOAT_EQ(element("sample")->width, 600.0f);
}

TEST_F(DynamicLayoutTest, ExportQueuesOrdinaryNetworkImagesBeforeInitialLayout) {
    const char* image_url = "https://example.test/radiant-export-ready.svg";
    const char* svg = "<svg xmlns='http://www.w3.org/2000/svg' width='73' height='41'>"
        "<rect width='73' height='41' fill='red'/></svg>";
    EnhancedFileCache* cache = enhanced_cache_create(host_path_dir(HOST_PATH_CACHE, "cache"),
        1024 * 1024, 10);
    ASSERT_NE(cache, nullptr);
    lam::Temp<char> cached_path(enhanced_cache_store(cache, image_url, svg, strlen(svg), nullptr));
    enhanced_cache_destroy(cache);
    ASSERT_NE(cached_path.get(), nullptr);
    struct Case { const char* markup; float width; float height; };
    const Case cases[] = {
        {"<img id='image' src='radiant-export-ready.svg'>", 73.0f, 41.0f},
        {"<video id='image' width='146' height='82' poster='radiant-export-ready.svg'></video>", 146.0f, 82.0f},
        {"<video id='image' poster='radiant-export-ready.svg'></video>", 73.0f, 41.0f},
        {"<video id='image' style='max-height:20.5px' poster='radiant-export-ready.svg'></video>", 36.5f, 20.5f},
        {"<video id='image' style='contain:size;contain-intrinsic-size:32px 48px' poster='radiant-export-ready.svg'></video>", 32.0f, 48.0f}
    };
    for (const Case& entry : cases) {
        SCOPED_TRACE(entry.markup);
        StrBuf* html = strbuf_create("<!doctype html><base href='https://example.test/'>"
            "<style>body{margin:0}</style>");
        strbuf_append_str(html, entry.markup);
        ASSERT_EQ(write_text_file_atomic("temp/export_network_image.html", html->str), 0);
        strbuf_free(html);
        struct SessionGuard {
            RenderExportSession value = {};
            ~SessionGuard() { render_export_session_finish(&value); }
        } session;
        ASSERT_TRUE(render_export_session_begin(&session.value, "temp/export_network_image.html",
            1200, 800, 1200, 800, 1.0f, false, false, 1.0f, false));
        DomDocument* exported = session.value.document.get();
        ASSERT_NE(exported, nullptr);
        DomElement* image = dom_find_element_by_id(exported->root, "image");
        ASSERT_NE(image, nullptr);
        EXPECT_FLOAT_EQ(image->width, entry.width);
        EXPECT_FLOAT_EQ(image->height, entry.height);
        ASSERT_NE(image->embed, nullptr);
        EXPECT_NE(image->tag_id == MARKUP_NAME_VIDEO
            ? image->embed->poster.get() : image->embed->img.get(), nullptr);
        EXPECT_DOUBLE_EQ(exported->js.virtual_clock_ms, 0.0);
        EXPECT_TRUE(render_export_session_finish(&session.value));
    }
    file_delete(cached_path.get());
}

TEST_F(DynamicLayoutTest, AlternateLinkedStylesheetRemainsAvailableButInactive) {
    ASSERT_EQ(write_text_file_atomic("temp/alternate_stylesheet.css",
        "#sample{width:8px}"), 0);
    load("<!doctype html><style>#sample{width:600px}</style>"
         "<link id='theme' rel='alternate stylesheet' title='Dark' href='alternate_stylesheet.css'>"
         "<div id='sample'>Visible content</div>");
    ASSERT_NE(element("sample"), nullptr);
    EXPECT_FLOAT_EQ(element("sample")->width, 600.0f);
    CssStylesheet* alternate = nullptr;
    for (int i = 0; i < document->stylesheet_count; i++) {
        if (document->stylesheets[i]->owner_element == element("theme")) {
            alternate = document->stylesheets[i];
        }
    }
    ASSERT_NE(alternate, nullptr);
    EXPECT_TRUE(alternate->disabled);
    alternate->disabled = false;
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, false);
    EXPECT_FLOAT_EQ(element("sample")->width, 8.0f);
}

TEST_F(DynamicLayoutTest, BackgroundColorWideKeywordsUseTransparentInitialValue) {
    load("<!doctype html><style>body{margin:0;background:#123456}"
         ".sample{width:100px;height:40px;background:red}"
         "#initial{background-color:initial}#unset{background-color:unset}"
         "#shorthand{background:initial}#inherited{background-color:inherit}"
         "#image{background-image:url(data:image/svg+xml,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%2F%3E);"
         "background-color:initial}"
         "</style><div id='initial' class='sample'></div>"
         "<div id='unset' class='sample'></div><div id='shorthand' class='sample'></div>"
         "<div id='inherited' class='sample'></div><div id='image' class='sample'></div>");
    for (const char* id : {"initial", "unset", "shorthand", "image"}) {
        SCOPED_TRACE(id);
        DomElement* sample = element(id);
        ASSERT_NE(sample, nullptr);
        ASSERT_NE(sample->boundary()->background, nullptr);
        EXPECT_EQ(sample->boundary()->background->color.a, 0);
    }
    ASSERT_NE(element("image")->boundary()->background->image, nullptr);
    Color inherited = element("inherited")->boundary()->background->color;
    EXPECT_EQ(inherited.r, 0x12);
    EXPECT_EQ(inherited.g, 0x34);
    EXPECT_EQ(inherited.b, 0x56);
    EXPECT_EQ(inherited.a, 255);
}

TEST_F(DynamicLayoutTest, DesktopViewportRetainsHostGeometryWithNumericMeta) {
    load("<!doctype html><meta name='viewport' content='width=480,height=320'>"
         "<style>@property --registered-size{syntax:'<length>';inherits:true;initial-value:1px}"
         ":root{--registered-size:50vw}body{margin:0}#sample{width:100vw;height:50vh}"
         "#registered{width:var(--registered-size)}"
         "#query{height:20px}@media(min-width:1000px){#query{height:40px}}"
         "</style><div id='sample'></div><div id='query'></div><div id='registered'></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_EQ(document->viewport.width, 480);
    EXPECT_EQ(document->viewport.height, 320);
    ASSERT_NE(element("sample"), nullptr);
    for (float width : {1200.0f, 800.0f}) {
        ui.viewport_width = width;
        ui.viewport_height = 600.0f;
        layout_html_doc(&ui, document, false);
        EXPECT_FLOAT_EQ(document->root->width, width);
        EXPECT_FLOAT_EQ(element("sample")->width, width);
        EXPECT_FLOAT_EQ(element("sample")->height, 300.0f);
        EXPECT_FLOAT_EQ(element("registered")->width, width * 0.5f);
        EXPECT_FLOAT_EQ(element("query")->height, width >= 1000.0f ? 40.0f : 20.0f);
    }
    document->viewport.width = 2000;
    document->viewport.height = 2000;
    layout_html_doc(&ui, document, false);
    EXPECT_FLOAT_EQ(element("sample")->width, 800.0f);
    EXPECT_FLOAT_EQ(element("sample")->height, 300.0f);
}

TEST_F(DynamicLayoutTest, DisabledLinkedThemeKeepsItsImportedRulesInactiveUntilEnabled) {
    ASSERT_EQ(write_text_file_atomic("temp/disabled_theme_import.css",
        "#sample{width:8px}"), 0);
    ASSERT_EQ(write_text_file_atomic("temp/disabled_theme.css",
        "@import 'disabled_theme_import.css';body{background:#202124}"), 0);
    load("<!doctype html><html><head><style>body{margin:0;background:white}"
         "#sample{width:600px;height:40px}</style>"
         "<link id='theme' rel='stylesheet' disabled href='disabled_theme.css'>"
         "</head><body id='body'><div id='sample'>Visible content</div></body></html>");
    ASSERT_NE(document, nullptr);
    DomElement* sample = element("sample");
    DomElement* body = element("body");
    DomElement* link = element("theme");
    ASSERT_TRUE(sample && body && link);
    EXPECT_FLOAT_EQ(sample->width, 600.0f);
    ASSERT_NE(body->boundary()->background, nullptr);
    EXPECT_EQ(body->boundary()->background->color.r, 255);
    CssStylesheet* theme = nullptr;
    for (int i = 0; i < document->stylesheet_count; i++) {
        CssStylesheet* sheet = document->stylesheets[i];
        if (sheet->owner_element == link && !sheet->is_import_child) theme = sheet;
    }
    ASSERT_NE(theme, nullptr);
    EXPECT_TRUE(theme->disabled);

    theme->disabled = false;
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, false);
    EXPECT_FLOAT_EQ(sample->width, 8.0f);
    EXPECT_EQ(body->boundary()->background->color.r, 32);

    theme->disabled = true;
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, false);
    EXPECT_FLOAT_EQ(sample->width, 600.0f);
    EXPECT_EQ(body->boundary()->background->color.r, 255);
}

TEST_F(DynamicLayoutTest, DisabledLinkAttributeUsesBooleanPresence) {
    ASSERT_EQ(write_text_file_atomic("temp/disabled_theme_boolean.css",
        "#sample{width:8px}"), 0);
    load("<!doctype html><style>#sample{width:600px}</style>"
         "<link rel='stylesheet' disabled='false' href='disabled_theme_boolean.css'>"
         "<div id='sample'>Visible content</div>");
    ASSERT_NE(element("sample"), nullptr);
    EXPECT_FLOAT_EQ(element("sample")->width, 600.0f);
}

TEST_F(DynamicLayoutTest, GridPlacementDoesNotSuppressColumnFlexContent) {
    load("<!doctype html><style>body{margin:0;font:16px/24px Arial}"
         "#parent{width:600px;display:flex;flex-direction:column}"
         "#subject{grid-area:content}h3{margin:0;font:40px/48px Arial}"
         "</style><div id='parent'><h3>Documentation</h3><div id='subject'>"
         "Find tutorials, user guides, code samples and API references."
         "</div></div>");
    DomElement* subject = element("subject");
    ASSERT_NE(subject, nullptr);
    EXPECT_EQ(subject->parent_item_kind(), DomElement::PARENT_ITEM_FLEX);
    EXPECT_FLOAT_EQ(subject->width, 600.0f);
    EXPECT_GT(subject->height, 0.0f);
    ASSERT_NE(subject->first_child, nullptr);
    EXPECT_GT(subject->first_child->width, 100.0f);
}

TEST_F(DynamicLayoutTest, ParentFormattingContextOwnsItemRoleAcrossRelayout) {
    load("<!doctype html><style>body{margin:0}"
         "#parent{display:flex;width:600px;height:80px;grid-template-columns:200px 400px}"
         "#first{grid-area:1/1;flex:1 1 0%;min-width:0}"
         "#second{grid-area:1/2;flex:2 1 0%;min-width:0}"
         "</style><div id='parent'><div id='first'>First</div>"
         "<div id='second'>Second</div></div>");
    DomElement* parent = element("parent");
    DomElement* first = element("first");
    DomElement* second = element("second");
    ASSERT_TRUE(parent && first && second);
    for (int pass = 0; pass < 4; pass++) {
        bool grid = pass % 2 != 0;
        parent->set_attribute("style", grid ? "display:grid" : "display:flex");
        radiant_recascade_document(document);
        layout_html_doc(&ui, document, false);
        SCOPED_TRACE(pass);
        EXPECT_EQ(first->parent_item_kind(), grid ? DomElement::PARENT_ITEM_GRID
            : DomElement::PARENT_ITEM_FLEX);
        EXPECT_FLOAT_EQ(first->width, 200.0f);
        EXPECT_FLOAT_EQ(second->width, 400.0f);
        EXPECT_FLOAT_EQ(first->height, 80.0f);
        EXPECT_FLOAT_EQ(second->height, 80.0f);
    }
}

TEST_F(DynamicLayoutTest, LegacyClipBoundsOwnPaintDescendantsAndNativeHits) {
    load("<!doctype html><style>html,body{margin:0;background:white}"
         "#box{position:absolute;left:20px;top:20px;width:100px;height:80px;"
         "background:red;clip:rect(10px,60px,50px,20px)}"
         "#child{position:absolute;left:25px;top:15px;width:10px;height:10px;"
         "background:blue;z-index:1}</style><div id='box'><div id='child'></div></div>");
    ASSERT_NE(document, nullptr);
    DomElement* box = element("box");
    ASSERT_NE(box, nullptr);
    const char* styles[] = {"", "clip:rect(auto 50px auto auto)",
        "clip:rect(60px,20px,10px,50px)", "clip:auto",
        "position:relative;clip:rect(0px,0px,0px,0px)",
        "position:fixed;clip:rect(-10px,60px,50px,-10px);clip-path:inset(20px)"};
    const float points[][2] = {{25,25}, {50,40}, {90,60}, {50,90}};
    const bool visible[][4] = {{false,true,false,false}, {true,true,false,true},
        {false,false,false,false}, {true,true,true,true}, {true,true,true,true},
        {false,true,false,false}};
    for (size_t phase = 0; phase < sizeof(styles) / sizeof(styles[0]); phase++) {
        SCOPED_TRACE(phase);
        if (phase) {
            ASSERT_TRUE(box->set_attribute("style", styles[phase]));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, true);
        }
        char output[96];
        snprintf(output, sizeof(output), "temp/ledger-next-clip-%zu.png", phase);
        render_html_doc(&ui, document->view_tree, output);
        ASSERT_FALSE(ui.render_failed);
        snprintf(output, sizeof(output), "temp/ledger-next-clip-%zu.svg", phase);
        char* svg = render_view_tree_to_svg(&ui, document->view_tree->root, 1200.0f, 800.0f);
        ASSERT_NE(svg, nullptr);
        EXPECT_TRUE(save_svg_to_file(svg, output));
        mem_free(svg);
        snprintf(output, sizeof(output), "temp/ledger-next-clip-%zu.png", phase);
        int width, height, channels;
        unsigned char* pixels = image_load(output, &width, &height, &channels, 4);
        ASSERT_NE(pixels, nullptr);
        for (size_t i = 0; i < 4; i++) {
            SCOPED_TRACE(i);
            const unsigned char* pixel = pixels +
                ((size_t)points[i][1] * (size_t)width + (size_t)points[i][0]) * 4;
            EXPECT_EQ(pixel[1], visible[phase][i] ? 0 : 255);
            // the clip rectangle covers both the owner's border box and lifted descendants.
            EventContext context = {};
            context.ui_context = &ui;
            context.target_document = document;
            context.event.type = RDT_EVENT_MOUSE_MOVE;
            context.event.mouse_position.x = context.viewport_pointer_x = points[i][0];
            context.event.mouse_position.y = context.viewport_pointer_y = points[i][1];
            target_html_doc(&context, document->view_tree);
            DomNode* hit = context.target;
            bool hit_box = hit == box || (hit && hit->parent == box);
            // clip-path hit testing is a separate existing conformance limit.
            if (phase != 5) EXPECT_EQ(hit_box, visible[phase][i]);
        }
        image_free(pixels);
    }
}

TEST_F(DynamicLayoutTest, LegacyClipResolvesLengthsInheritanceAndAutoEdgesAfterSizing) {
    load("<!doctype html><style>html,body{margin:0}#parent{font-size:10px;"
         "--edge:2em;clip:rect(calc(1em + 2px),auto,auto,var(--edge))}"
         "#child{position:absolute;width:100px;height:80px;clip:inherit}"
         "</style><div id='parent'><div id='child'></div></div>");
    ASSERT_NE(document, nullptr);
    DomElement* parent = element("parent");
    DomElement* child = element("child");
    ASSERT_NE(parent, nullptr);
    ASSERT_NE(child, nullptr);
    Bound clip;
    EXPECT_FALSE(render_clip_legacy_geometry(lam::view_require_block(parent), &clip));
    ASSERT_TRUE(render_clip_legacy_geometry(lam::view_require_block(child), &clip));
    EXPECT_FLOAT_EQ(clip.left, 20.0f);
    EXPECT_FLOAT_EQ(clip.top, 12.0f);
    EXPECT_FLOAT_EQ(clip.right, 100.0f);
    EXPECT_FLOAT_EQ(clip.bottom, 80.0f);
    ASSERT_TRUE(parent->set_attribute("style", "font-size:20px"));
    ASSERT_TRUE(child->set_attribute("style", "width:150px;height:90px"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    ASSERT_TRUE(render_clip_legacy_geometry(lam::view_require_block(child), &clip));
    EXPECT_FLOAT_EQ(clip.left, 40.0f);
    EXPECT_FLOAT_EQ(clip.top, 22.0f);
    EXPECT_FLOAT_EQ(clip.right, 150.0f);
    EXPECT_FLOAT_EQ(clip.bottom, 90.0f);
    ASSERT_TRUE(child->set_attribute("style", "clip:unset"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    EXPECT_FALSE(render_clip_legacy_geometry(lam::view_require_block(child), &clip));
}

TEST_F(DynamicLayoutTest, ColumnFlexMeasuresNestedWrappingRows) {
    load_nested_flex_toolbar("wrap", "wrap");
    ASSERT_NE(document, nullptr);
    check_flex_toolbar(102.0f);
    DomElement* inner = element("bar")->first_child->next_sibling->as_element();
    ASSERT_NE(inner, nullptr);
    ASSERT_TRUE(inner->set_attribute("style", "max-width:300px"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    check_flex_toolbar(96.0f);
}

TEST_F(DynamicLayoutTest, ColumnFlexMeasuresNowrapToolbarWithWrappingChild) {
    load_nested_flex_toolbar("nowrap", "wrap");
    ASSERT_NE(document, nullptr);
    check_flex_toolbar(96.0f);
}

TEST_F(DynamicLayoutTest, ColumnFlexMeasuresWrappingToolbarWithNowrapChild) {
    load_nested_flex_toolbar("wrap", "nowrap");
    ASSERT_NE(document, nullptr);
    check_flex_toolbar(74.0f);
}

TEST_F(DynamicLayoutTest, HtmlColorAttributesUseNamedCssColors) {
    load("<!doctype html><body id='body' bgcolor='navy'>"
         "<font id='font' color='ReD'>Text</font>"
         "<table><tr><td id='cell' bgcolor='lime'>Cell</td></tr></table>");
    ASSERT_NE(document, nullptr);
    for (const char* id : {"body", "cell"}) {
        DomElement* node = element(id);
        ASSERT_NE(node, nullptr);
        ASSERT_NE(node->bound, nullptr);
        ASSERT_NE(node->bound->background, nullptr);
        Color color = node->bound->background->color;
        EXPECT_EQ(color.r, 0);
        EXPECT_EQ(color.g, strcmp(id, "cell") == 0 ? 255 : 0);
        EXPECT_EQ(color.b, strcmp(id, "body") == 0 ? 128 : 0);
        EXPECT_EQ(color.a, 255);
    }
    DomElement* font = element("font");
    ASSERT_NE(font, nullptr);
    ASSERT_NE(font->in_line, nullptr);
    EXPECT_EQ(font->in_line->color.r, 255);
    EXPECT_EQ(font->in_line->color.g, 0);
    EXPECT_EQ(font->in_line->color.b, 0);
    EXPECT_TRUE(font->in_line->has_color);
}

TEST_F(DynamicLayoutTest, NormalFontFaceRegistersTheNormalWeight) {
    load("<!doctype html><style>@font-face{font-family:LedgerNormal;"
         "src:url('../test/layout/data/font/Ahem.ttf');font-weight:normal}"
         "</style><div>Text</div>");
    ASSERT_NE(document, nullptr);
    FontStyleDesc style = {};
    style.family = "LedgerNormal";
    style.size_px = 16.0f;
    style.weight = FONT_WEIGHT_NORMAL;
    style.slant = FONT_SLANT_NORMAL;
    const FontFaceDesc* face = font_face_find(ui.font_ctx, &style);
    ASSERT_NE(face, nullptr);
    EXPECT_EQ(face->weight, FONT_WEIGHT_NORMAL);
}

TEST_F(DynamicLayoutTest, SlicedTextBoxTrimRestoresContinuationLeading) {
    load("<!doctype html><style>@font-face{font-family:Ahem;"
         "src:url('../test/layout/data/font/Ahem.ttf')}html,body{margin:0}"
         "#columns{columns:3;column-fill:auto;border:1px solid;width:800px;"
         "height:360px;font:40px/80px Ahem;orphans:1;widows:1}"
         "#trim{text-box-trim:trim-both;text-box-edge:text}"
         "</style><div id='columns'><div id='trim'>"
         "abc abc abc abc abc abc abc abc abc abc</div></div>");
    DomElement* trim = element("trim");
    ASSERT_NE(trim, nullptr);
    ViewText* text = lam::view_require<RDT_VIEW_TEXT>(trim->first_child);
    int index = 0;
    for (TextRect* rect = text->rect; rect; rect = rect->next, index++) {
        SCOPED_TRACE(index);
        float expected = index < 4 ? index * 80.0f :
            20.0f + ((index - 4) % 4) * 80.0f;
        EXPECT_FLOAT_EQ(rect->y, expected);
    }
    EXPECT_EQ(index, 10);
}

TEST_F(DynamicLayoutTest, NestedStartTrimUsesTheInnermostRequest) {
    load("<!doctype html><style>@font-face{font-family:Ahem;"
         "src:url('../test/layout/data/font/Ahem.ttf')}html,body{margin:0}"
         "#columns{columns:3;column-fill:auto;border:1px solid;width:800px;"
         "height:290px;font:40px/80px Ahem;orphans:1;widows:1;text-box-trim:trim-both}"
         "#trim{text-box-trim:trim-start;text-box-edge:text}"
         "</style><div id='columns'><div id='trim'>"
         "abc abc abc abc abc abc abc abc abc</div><div id='spacer' style='height:100px'></div></div>");
    DomElement* trim = element("trim");
    ASSERT_NE(trim, nullptr);
    ViewText* text = lam::view_require<RDT_VIEW_TEXT>(trim->first_child);
    ASSERT_NE(text->rect, nullptr);
    EXPECT_FLOAT_EQ(trim->y + text->rect->y, 1.0f);
    ASSERT_NE(element("spacer"), nullptr);
    EXPECT_FLOAT_EQ(element("spacer")->y, 161.0f);
}

TEST_F(DynamicLayoutTest, MulticolStartTrimAppliesToContinuationColumns) {
    load("<!doctype html><style>@font-face{font-family:Ahem;"
         "src:url('../test/layout/data/font/Ahem.ttf')}html,body{margin:0}"
         "#columns{columns:3;column-fill:auto;border:1px solid;width:800px;"
         "height:240px;font:40px/80px Ahem;orphans:1;widows:1;text-box-trim:trim-both}"
         "#trim{text-box-trim:trim-end;text-box-edge:text}.spacer{height:5px}"
         "</style><div id='columns'><div class='spacer'></div><div id='trim'>"
         "abc abc abc abc abc abc abc</div><div class='spacer'></div></div>");
    DomElement* trim = element("trim");
    ASSERT_NE(trim, nullptr);
    ViewText* text = lam::view_require<RDT_VIEW_TEXT>(trim->first_child);
    int index = 0;
    for (TextRect* rect = text->rect; rect; rect = rect->next, index++) {
        SCOPED_TRACE(index);
        float expected = index < 2 ? 26.0f + index * 80.0f :
            1.0f + ((index - 2) % 3) * 80.0f;
        EXPECT_FLOAT_EQ(trim->y + rect->y, expected);
    }
    EXPECT_EQ(index, 7);
}

TEST_F(DynamicLayoutTest, ClosingPunctuationCanFitByTrimmingItsLineEnd) {
    load("<!doctype html><style>@font-face{font-family:TrimTest;"
         "src:url('../test/layout/data/support/fonts/noto/cjk/NotoSansCJKjp-Regular-subset-halt.otf')}"
         "#trim{font:20px TrimTest;width:72px;text-spacing-trim:trim-start}"
         "</style><div id='trim'>国国国）国国国）</div>");
    DomElement* trim = element("trim");
    ASSERT_NE(trim, nullptr);
    ViewText* text = lam::view_require<RDT_VIEW_TEXT>(trim->first_child);
    ASSERT_NE(text->rect, nullptr);
    EXPECT_EQ(text->rect->length, strlen("国国国）"));
    ASSERT_NE(text->rect->next, nullptr);
    EXPECT_EQ(text->rect->next->length, strlen("国国国）"));
    EXPECT_EQ(text->rect->next->next, nullptr);
    EXPECT_FLOAT_EQ(text->rect->width, 70.0f);
}

TEST_F(DynamicLayoutTest, CjkHyphenBreakingRespectsExplicitScriptSubtags) {
    load("<!doctype html><div id='ja' lang='ja'></div><div id='jp' lang='JA-jp'></div>"
         "<div id='hang' lang='ja-Hang'></div><div id='latin' lang='zh-Latn'></div>"
         "<div id='zh' lang='zh-Hant'></div><div id='ko' lang='ko'></div>"
         "<div id='hani' lang='ko-Hani'></div><div id='extension' lang='ja-x-Hang'></div>"
         "<div id='hanb' lang='zh-Hanb'></div><div id='bopo' lang='zh-Bopo'></div>"
         "<div id='hira' lang='ja-Hira'></div><div id='kana' lang='en-Kana'></div>");
    for (const char* id : {"ja", "jp", "hang", "latin", "zh", "ko", "hani", "extension",
                            "hanb", "bopo", "hira", "kana"}) {
        SCOPED_TRACE(id);
        DomElement* owner = element(id);
        ASSERT_NE(owner, nullptr);
        bool allows = strcmp(id, "hang") != 0 && strcmp(id, "latin") != 0 && strcmp(id, "ko") != 0;
        for (uint32_t punctuation : {0x301Cu, 0x30A0u}) {
            for (CssEnum mode : {CSS_VALUE_NORMAL, CSS_VALUE_LOOSE, CSS_VALUE_STRICT}) {
                EXPECT_EQ(layout_text_allows_letter_break(0x4EAC, punctuation, mode,
                    false, false, owner), allows && mode != CSS_VALUE_STRICT);
            }
        }
    }
}

TEST_F(DynamicLayoutTest, JapaneseWaveDashBreaksInFlowAndAcrossInlineSeams) {
    load("<!doctype html><style>@font-face{font-family:CjkBreak;"
         "src:url('../test/layout/data/font/NotoSansSC-Subset.otf')}"
         "div{font:20px/20px CjkBreak;width:40px;line-break:loose}"
         "</style><div id='plain' lang='ja'>東京〜大阪</div>"
         "<div id='seam' lang='ja'><span>東京</span><span>〜大阪</span></div>"
         "<div id='minimum' lang='ja' style='width:min-content'>東京〜大阪</div>");
    DomElement* plain = element("plain");
    DomElement* seam = element("seam");
    DomElement* minimum = element("minimum");
    ASSERT_NE(plain, nullptr);
    ASSERT_NE(seam, nullptr);
    ASSERT_NE(minimum, nullptr);
    ViewText* text = lam::view_require<RDT_VIEW_TEXT>(plain->first_child);
    ASSERT_NE(text->rect, nullptr);
    EXPECT_EQ(text->rect->length, strlen("東京"));
    ASSERT_NE(text->rect->next, nullptr);
    EXPECT_EQ(text->rect->next->length, strlen("〜大"));
    EXPECT_FLOAT_EQ(plain->height, 60.0f);
    EXPECT_FLOAT_EQ(seam->height, plain->height);
    EXPECT_FLOAT_EQ(minimum->width, 20.0f);
}

TEST_F(DynamicLayoutTest, FloatPrescanStopsAtAnEmptyListItem) {
    load("<!doctype html><style>#host{width:200px}"
         "#item{display:list-item;height:60px}#float{float:right;width:20px;height:20px}"
         "</style><div id='host'><div id='item'></div><div id='float'></div></div>");
    DomElement* item = element("item");
    DomElement* floated = element("float");
    ASSERT_NE(item, nullptr);
    ASSERT_NE(floated, nullptr);
    EXPECT_FLOAT_EQ(floated->y, item->y + item->height);
}

TEST_F(DynamicLayoutTest, OrthogonalFallbackUsesScrollportAcrossContainment) {
    StrBuf* source = strbuf_create("<!doctype html><style>html{font:15px/1 sans-serif;writing-mode:vertical-lr}"
         "article{writing-mode:horizontal-tb;height:50vh;overflow:hidden}"
         "aside{writing-mode:vertical-rl;width:2em;margin:4em 0;padding:2em 0;"
         "border:4px solid;overflow:hidden}div{width:calc(2em + 8px)}"
         ".middle{contain:layout size}.scroll{contain:layout size}.large{height:120vh}"
         "</style>");
    const char* ids[] = {"middle", "scroll", "large"};
    for (const char* id : ids) {
        strbuf_append_format(source, "<article class='%s'><div class='%s'><aside id='%s'>",
            id, strcmp(id, "scroll") == 0 ? "" : "middle", id);
        for (int repeat = 0; repeat < 8; repeat++)
            strbuf_append_str(source, "abcde fghijk lmnop qrstu vwxyz ");
        strbuf_append_str(source, "</aside></div></article>");
    }
    load(source->str);
    strbuf_free(source);
    for (const char* id : {"middle", "scroll", "large"}) {
        SCOPED_TRACE(id);
        DomElement* aside = element(id);
        ASSERT_NE(aside, nullptr);
        // containment does not remove the nearest scrollport's fixed inline constraint.
        EXPECT_FLOAT_EQ(aside->height, strcmp(id, "large") == 0 ? 680.0f : 280.0f);
    }
}

TEST_F(DynamicLayoutTest, GridLastBaselineUsesTheLastLine) {
    load("<!doctype html><style>html,body{margin:0}"
         ".grid{display:grid;grid-template-columns:100px 100px;font:20px/20px serif}"
         "#first{align-items:first baseline}#last{align-items:last baseline}"
         "</style><div class='grid' id='first'>"
         "<div id='firstTall'>One<br>Two<br>Three</div><div id='firstShort'>One</div></div>"
         "<div class='grid' id='last'>"
         "<div id='lastTall'>One<br>Two<br>Three</div><div id='lastShort'>One</div></div>");
    ASSERT_NE(document, nullptr);
    for (const char* prefix : {"first", "last"}) {
        char tall_id[24], short_id[24];
        snprintf(tall_id, sizeof(tall_id), "%sTall", prefix);
        snprintf(short_id, sizeof(short_id), "%sShort", prefix);
        DomElement* tall = element(tall_id);
        DomElement* short_item = element(short_id);
        ASSERT_NE(tall, nullptr);
        ASSERT_NE(short_item, nullptr);
        EXPECT_FLOAT_EQ(tall->height, 60.0f);
        EXPECT_FLOAT_EQ(short_item->height, 20.0f);
        EXPECT_FLOAT_EQ(short_item->y - tall->y,
            strcmp(prefix, "last") == 0 ? 40.0f : 0.0f);
    }
    ASSERT_TRUE(element("last")->set_attribute("style", "align-items:first baseline"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    EXPECT_FLOAT_EQ(element("lastShort")->y, element("lastTall")->y);
    EXPECT_FLOAT_EQ(element("lastShort")->height, 20.0f);
}

TEST_F(DynamicLayoutTest, GridKeepsFirstAndLastBaselineGroupsSeparate) {
    load("<!doctype html><style>html,body{margin:0}"
         "#grid{display:grid;grid-template-columns:100px 100px 100px 100px;"
         "grid-template-rows:100px;font:20px/20px serif}"
         ".first{align-self:first baseline}.last{align-self:last baseline}"
         "</style><div id='grid'>"
         "<div id='firstTall' class='first'>One<br>Two<br>Three</div>"
         "<div id='firstShort' class='first'>One</div>"
         "<div id='lastTall' class='last'>One<br>Two</div>"
         "<div id='lastShort' class='last'>One</div></div>");
    ASSERT_NE(document, nullptr);
    const struct { const char* id; float y; float height; } expected[] = {
        {"firstTall", 0.0f, 60.0f}, {"firstShort", 0.0f, 20.0f},
        {"lastTall", 60.0f, 40.0f}, {"lastShort", 80.0f, 20.0f},
    };
    for (const auto& entry : expected) {
        SCOPED_TRACE(entry.id);
        DomElement* child = element(entry.id);
        ASSERT_NE(child, nullptr);
        EXPECT_FLOAT_EQ(child->y, entry.y);
        EXPECT_FLOAT_EQ(child->height, entry.height);
    }
}

TEST_F(DynamicLayoutTest, FlexFirstAndLastBaselinesUseSeparateLineMetrics) {
    load("<!doctype html><style>html,body{margin:0}"
         ".row{display:flex;width:200px;font:20px/20px serif}.row>div{width:100px;flex:none}"
         "#first{align-items:first baseline}#last{align-items:last baseline}"
         "#fixed{align-items:last baseline;height:100px}"
         "</style><div class='row' id='first'>"
         "<div id='firstTall'>One<br>Two<br>Three</div><div id='firstShort'>One</div></div>"
         "<div class='row' id='last'>"
         "<div id='lastTall'>One<br>Two<br>Three</div><div id='lastShort'>One</div></div>"
         "<div class='row' id='fixed'>"
         "<div id='fixedTall'>One<br>Two<br>Three</div><div id='fixedShort'>One</div></div>");
    ASSERT_NE(document, nullptr);
    for (const char* prefix : {"first", "last", "fixed"}) {
        SCOPED_TRACE(prefix);
        char tall_id[24], short_id[24];
        snprintf(tall_id, sizeof(tall_id), "%sTall", prefix);
        snprintf(short_id, sizeof(short_id), "%sShort", prefix);
        DomElement* tall = element(tall_id);
        DomElement* short_item = element(short_id);
        ASSERT_NE(tall, nullptr);
        ASSERT_NE(short_item, nullptr);
        EXPECT_FLOAT_EQ(tall->height, 60.0f);
        EXPECT_FLOAT_EQ(short_item->height, 20.0f);
        EXPECT_FLOAT_EQ(tall->y, strcmp(prefix, "fixed") == 0 ? 40.0f : 0.0f);
        EXPECT_FLOAT_EQ(short_item->y - tall->y,
            strcmp(prefix, "first") == 0 ? 0.0f : 40.0f);
    }
    ASSERT_TRUE(element("last")->set_attribute("style", "align-items:first baseline"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    EXPECT_FLOAT_EQ(element("lastShort")->y, element("lastTall")->y);
}

TEST_F(DynamicLayoutTest, FlexKeepsFirstAndLastBaselineGroupsSeparate) {
    load("<!doctype html><style>html,body{margin:0}"
         "#row{display:flex;flex-direction:row-reverse;width:400px;height:100px;font:20px/20px serif}"
         "#row>div{width:100px;flex:none}.first{align-self:first baseline}.last{align-self:last baseline}"
         "</style><div id='row'><div id='firstTall' class='first'>One<br>Two<br>Three</div>"
         "<div id='firstShort' class='first'>One</div><div id='lastTall' class='last'>One<br>Two</div>"
         "<div id='lastShort' class='last'>One</div></div>");
    ASSERT_NE(document, nullptr);
    const struct { const char* id; float y; float height; } expected[] = {
        {"firstTall", 0.0f, 60.0f}, {"firstShort", 0.0f, 20.0f},
        {"lastTall", 60.0f, 40.0f}, {"lastShort", 80.0f, 20.0f},
    };
    for (const auto& entry : expected) {
        SCOPED_TRACE(entry.id);
        DomElement* child = element(entry.id);
        ASSERT_NE(child, nullptr);
        EXPECT_FLOAT_EQ(child->y, entry.y);
        EXPECT_FLOAT_EQ(child->height, entry.height);
    }
}

TEST_F(DynamicLayoutTest, FlexLastBaselineRespectsWrappedLinesAndAutoMargins) {
    load("<!doctype html><style>html,body{margin:0}.row{display:flex;font:20px/20px serif;"
         "align-items:last baseline;height:200px;width:200px;flex-wrap:wrap;align-content:stretch}"
         ".row>div{width:100px;flex:none}#auto{height:100px;width:300px;flex-wrap:nowrap}"
         "</style><div class='row'><div id='a'>One<br>Two<br>Three</div><div id='b'>One</div>"
         "<div id='c'>One<br>Two</div><div id='d'>One</div></div>"
         "<div class='row' id='auto'><div id='tall'>One<br>Two<br>Three</div>"
         "<div id='margin' style='margin-top:auto'>One</div>"
         "<div id='start' style='align-self:start'>One</div></div>");
    ASSERT_NE(document, nullptr);
    const struct { const char* id; float y; } expected[] = {
        {"a", 50.0f}, {"b", 90.0f}, {"c", 160.0f}, {"d", 180.0f},
        {"tall", 40.0f}, {"margin", 80.0f}, {"start", 0.0f},
    };
    for (const auto& entry : expected) {
        SCOPED_TRACE(entry.id);
        ASSERT_NE(element(entry.id), nullptr);
        EXPECT_FLOAT_EQ(element(entry.id)->y, entry.y);
    }
}

TEST_F(DynamicLayoutTest, NestedColumnWithoutBaselineGroupUsesItsFirstChild) {
    load("<!doctype html><style>html,body{margin:0}div{display:flex}"
         "#row{width:100px;height:100px;align-items:baseline}"
         ".column{flex-direction:column}</style><div id='row'>"
         "<div style='width:50px;height:60px'></div><div id='column' class='column'>"
         "<div style='width:50px;height:80px' class='column'>"
         "<div style='width:50px;height:30px' class='column'></div>"
         "<div style='width:50px;height:40px' class='column'></div></div></div></div>");
    ASSERT_NE(document, nullptr);
    ASSERT_NE(element("column"), nullptr);
    EXPECT_FLOAT_EQ(element("column")->y, 30.0f);
    EXPECT_FLOAT_EQ(element("column")->height, 80.0f);
}

TEST_F(DynamicLayoutTest, WhitespaceCacheTracksOverridesAndRetainedStyleChanges) {
    load("<!doctype html><style>html,body{margin:0}#host,#ua{font:10px/10px monospace}</style>"
         "<div id='host' style='white-space:pre'>A  B\nC<span id='override' style='white-space:normal'> D  E</span></div>"
         "<pre id='ua'>A  B\nC</pre>");
    ASSERT_NE(document, nullptr);
    LayoutContext context = {};
    context.doc = lam::up(document);
    DomNode* inherited = element("host")->first_child;
    DomNode* overridden = element("override")->first_child;
    DomNode* ua = element("ua")->first_child;
    ASSERT_NE(inherited, nullptr);
    ASSERT_NE(overridden, nullptr);
    ASSERT_NE(ua, nullptr);
    for (int query = 0; query < 3; query++)
        EXPECT_EQ(get_white_space_value(inherited, &context), CSS_VALUE_PRE);
    EXPECT_EQ(get_white_space_value(overridden, &context), CSS_VALUE_NORMAL);
    EXPECT_EQ(get_white_space_value(ua, &context), CSS_VALUE_PRE);
    EXPECT_FLOAT_EQ(element("host")->height, 20.0f);
    EXPECT_FLOAT_EQ(element("ua")->height, 20.0f);
    EXPECT_EQ(get_white_space_value(inherited, &context), CSS_VALUE_PRE);
    ASSERT_TRUE(element("host")->set_attribute("style", "white-space:normal"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    EXPECT_EQ(get_white_space_value(inherited, &context), CSS_VALUE_NORMAL);
    EXPECT_EQ(get_white_space_value(overridden, &context), CSS_VALUE_NORMAL);
    EXPECT_EQ(get_white_space_value(ua, &context), CSS_VALUE_PRE);
    EXPECT_FLOAT_EQ(element("host")->height, 10.0f);
    EXPECT_FLOAT_EQ(element("ua")->height, 20.0f);
}

TEST_F(DynamicLayoutTest, VideoPosterPaintsWithoutSourceAndSurvivesReplacement) {
    ASSERT_EQ(write_text_file_atomic("temp/ledger-impl-poster.svg",
        "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50'>"
        "<rect width='50' height='50' fill='red'/><rect x='50' width='50' height='50' fill='lime'/></svg>"), 0);
    ASSERT_EQ(write_text_file_atomic("temp/ledger-impl-poster-blue.svg",
        "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50'>"
        "<rect width='100' height='50' fill='blue'/></svg>"), 0);
    load("<!doctype html><style>html,body{margin:0}video{display:block;width:100px;height:100px;"
         "object-fit:contain;background:#123456}</style>"
         "<video id='video' poster='ledger-impl-poster.svg'></video>");
    ASSERT_NE(document, nullptr);
    DomElement* video = element("video");
    ASSERT_NE(video, nullptr);
    ASSERT_NE(video->embed, nullptr);
    ASSERT_NE(video->embed->poster, nullptr);
    ViewBlock* view = lam::view_require_block(static_cast<View*>(video));
    EXPECT_EQ(render_media_image_surface(view), video->embed->poster.get());
    for (int phase = 0; phase < 4; phase++) {
        SCOPED_TRACE(phase);
        if (phase > 0) {
            ASSERT_TRUE(video->set_attribute("poster", phase == 1 ? "ledger-impl-poster-blue.svg"
                : phase == 2 ? "" : "ledger-impl-no-such-poster.svg"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, true);
        }
        char output[80];
        snprintf(output, sizeof(output), "temp/ledger-impl-poster-native-%d.png", phase);
        render_html_doc(&ui, document->view_tree, output);
        ASSERT_FALSE(ui.render_failed);
        int width, height, channels;
        unsigned char* pixels = image_load(output, &width, &height, &channels, 4);
        ASSERT_NE(pixels, nullptr);
        ASSERT_GE(width, 100);
        ASSERT_GE(height, 100);
        const unsigned char* left = pixels + (50 * width + 25) * 4;
        const unsigned char* right = pixels + (50 * width + 75) * 4;
        const unsigned char* letterbox = pixels + (10 * width + 25) * 4;
        EXPECT_EQ(left[0], phase == 0 ? 255 : phase == 1 ? 0 : 0x12);
        EXPECT_EQ(left[1], phase < 2 ? 0 : 0x34);
        EXPECT_EQ(left[2], phase == 0 ? 0 : phase == 1 ? 255 : 0x56);
        EXPECT_EQ(right[0], phase < 2 ? 0 : 0x12);
        EXPECT_EQ(right[1], phase == 0 ? 255 : phase == 1 ? 0 : 0x34);
        EXPECT_EQ(right[2], phase == 0 ? 0 : phase == 1 ? 255 : 0x56);
        EXPECT_EQ(letterbox[0], 0x12);
        EXPECT_EQ(letterbox[1], 0x34);
        EXPECT_EQ(letterbox[2], 0x56);
        image_free(pixels);
    }
    EXPECT_EQ(video->embed->poster, nullptr);
}

TEST_F(DynamicLayoutTest, VideoPosterUsesImagePaintBeforePreloadedPlayback) {
    ASSERT_EQ(write_text_file_atomic("temp/ledger-impl-preload-poster.svg",
        "<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'>"
        "<rect width='10' height='10' fill='red'/></svg>"), 0);
    load("<!doctype html><style>html,body{margin:0}video{display:block;width:100px;height:100px}</style>"
         "<video id='video' preload='none' src='../test/media/test_video_audio.mp4' "
         "poster='ledger-impl-preload-poster.svg'></video>");
    ASSERT_NE(document, nullptr);
    DomElement* video = element("video");
    ASSERT_NE(video, nullptr);
    ASSERT_NE(video->embed, nullptr);
    ASSERT_NE(video->embed->poster, nullptr);
    ViewBlock* view = lam::view_require_block(static_cast<View*>(video));
    EXPECT_EQ(render_media_image_surface(view), video->embed->poster.get());
    render_html_doc(&ui, document->view_tree, "temp/ledger-impl-preload-native.png");
    ASSERT_FALSE(ui.render_failed);
    int width, height, channels;
    unsigned char* pixels = image_load("temp/ledger-impl-preload-native.png", &width, &height, &channels, 4);
    ASSERT_NE(pixels, nullptr);
    const unsigned char* pixel = pixels + (50 * width + 50) * 4;
    EXPECT_EQ(pixel[0], 255);
    EXPECT_EQ(pixel[1], 0);
    EXPECT_EQ(pixel[2], 0);
    image_free(pixels);
}

TEST_F(DynamicLayoutTest, VideoPosterSharesImageFitAndAncestorClip) {
    ASSERT_EQ(write_text_file_atomic("temp/ledger-impl-clipped-poster.svg",
        "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='50'>"
        "<rect width='50' height='50' fill='red'/><rect x='50' width='50' height='50' fill='lime'/></svg>"), 0);
    load("<!doctype html><style>html,body{margin:0;background:white}"
         "#clip{width:75px;height:100px;overflow:hidden}"
         "video{display:block;width:100px;height:100px;object-fit:cover}"
         "</style><div id='clip'><video poster='ledger-impl-clipped-poster.svg'></video></div>");
    ASSERT_NE(document, nullptr);
    render_html_doc(&ui, document->view_tree, "temp/ledger-impl-poster-clipped.png");
    ASSERT_FALSE(ui.render_failed);
    int width, height, channels;
    unsigned char* pixels = image_load("temp/ledger-impl-poster-clipped.png", &width, &height, &channels, 4);
    ASSERT_NE(pixels, nullptr);
    const unsigned char* left = pixels + (10 * width + 25) * 4;
    const unsigned char* right = pixels + (10 * width + 60) * 4;
    const unsigned char* outside = pixels + (10 * width + 85) * 4;
    EXPECT_EQ(left[0], 255); EXPECT_EQ(left[1], 0);
    EXPECT_EQ(right[0], 0); EXPECT_EQ(right[1], 255);
    EXPECT_EQ(outside[0], 255); EXPECT_EQ(outside[1], 255); EXPECT_EQ(outside[2], 255);
    image_free(pixels);
}

TEST_F(DynamicLayoutTest, StandardSpaceEvenlyDistributesEdgeAndInteriorGaps) {
    load("<!doctype html><style>html,body{margin:0}"
         "#row{display:flex;justify-content:space-evenly;width:400px}"
         "#row>div{width:40px;height:10px;flex:none}</style>"
         "<div id='row'><div id='one'></div><div id='two'></div><div id='three'></div></div>");
    ASSERT_NE(document, nullptr);
    for (float width : {400.0f, 200.0f}) {
        if (width == 200.0f) {
            ASSERT_TRUE(element("row")->set_attribute("style", "width:200px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, true);
        }
        float gap = (width - 120.0f) / 4.0f;
        float expected_x = gap;
        for (const char* id : {"one", "two", "three"}) {
            DomElement* child = element(id);
            ASSERT_NE(child, nullptr);
            EXPECT_FLOAT_EQ(child->x, expected_x);
            expected_x += 40.0f + gap;
        }
    }
}

TEST_F(DynamicLayoutTest, FloatPrescanUsesActualGeometryAndFlowOrder) {
    load("<!doctype html><style>html,body{margin:0}.section{width:320px;height:100px}"
         ".floating{float:left;width:180px;height:30px}"
         ".box{display:inline-block;width:100px;height:10px;vertical-align:top}"
         "</style><div class='section'><div id='early' class='floating'></div>"
         "<span id='earlyBox' class='box'></span></div>"
         "<div class='section'><span></span><div id='empty' class='floating'></div>"
         "<span id='emptyBox' class='box'></span></div>"
         "<div class='section'><div style='height:20px'></div>"
         "<div id='late' class='floating'></div><span id='lateBox' class='box'></span></div>");
    ASSERT_NE(document, nullptr);
    for (float width : {180.0f, 40.0f, 220.0f}) {
        for (const char* id : {"early", "empty", "late"}) {
            SCOPED_TRACE(::testing::Message() << id << " width=" << width);
            DomElement* floating = element(id);
            ASSERT_NE(floating, nullptr);
            if (width != 180.0f) {
                char style[48];
                snprintf(style, sizeof(style), "width:%.0fpx", width);
                ASSERT_TRUE(floating->set_attribute("style", style));
            }
        }
        if (width != 180.0f) {
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, true);
        }
        for (const char* id : {"early", "empty", "late"}) {
            SCOPED_TRACE(::testing::Message() << id << " width=" << width);
            DomElement* floating = element(id);
            char box_id[24];
            snprintf(box_id, sizeof(box_id), "%sBox", id);
            DomElement* box = element(box_id);
            ASSERT_NE(box, nullptr);
            EXPECT_FLOAT_EQ(floating->width, width);
            EXPECT_FLOAT_EQ(floating->x, 0.0f);
            EXPECT_FLOAT_EQ(floating->y, strcmp(id, "late") == 0 ? 20.0f : 0.0f);
            EXPECT_EQ(floating->float_prelaid(), strcmp(id, "late") != 0);
            EXPECT_FLOAT_EQ(box->x, width);
        }
    }
}

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

TEST_F(DynamicLayoutTest, AbsoluteInsetsTransferPreferredAspectRatio) {
    load("<!doctype html><style>body{margin:0}div{display:flex;box-sizing:border-box}"
         "#parent{position:absolute;width:400px;height:300px}"
         "#child{position:absolute;inset:5%;aspect-ratio:3}</style>"
         "<div id='parent'><div id='child'></div></div>");
    ASSERT_NE(document, nullptr);
    ASSERT_NE(element("child"), nullptr);
    EXPECT_FLOAT_EQ(element("child")->width, 360.0f);
    EXPECT_FLOAT_EQ(element("child")->height, 120.0f);
}

TEST_F(DynamicLayoutTest, ColumnFlexTransfersStretchedWidthThroughAspectRatio) {
    load("<!doctype html><style>.parent{position:absolute;display:flex;flex-direction:column;"
         "width:100px;height:100px}.parent>div{display:flex;aspect-ratio:2}</style>"
         "<div class='parent'><div id='min-width' style='min-width:40px'></div></div>"
         "<div class='parent'><div id='min-height' style='min-height:40px'></div></div>"
         "<div class='parent' style='align-items:start'>"
         "<div id='start' style='min-width:40px'></div></div>"
         "<div class='parent'><div id='max-width' style='max-width:40px'></div></div>");
    ASSERT_NE(document, nullptr);
    const char* ids[] = {"min-width", "min-height", "start", "max-width"};
    const float widths[] = {100.0f, 100.0f, 40.0f, 40.0f};
    for (int i = 0; i < 4; i++) {
        SCOPED_TRACE(ids[i]);
        ASSERT_NE(element(ids[i]), nullptr);
        EXPECT_FLOAT_EQ(element(ids[i])->width, widths[i]);
        EXPECT_FLOAT_EQ(element(ids[i])->height, widths[i] / 2.0f);
    }
}

TEST_F(DynamicLayoutTest, ColumnFlexMinimumHeightDoesNotShrinkNaturalContents) {
    load("<!doctype html><style>body{margin:0}.column{display:flex;flex-direction:column;min-height:800px}"
         "#header{overflow:hidden;padding:20px 0}#header div{height:300px}"
         "#main div{height:1000px}.footer{height:100px}#grow{flex:1}</style>"
         "<div id='tall' class='column'><header id='header'><div></div></header>"
         "<main id='main'><div></div></main><footer id='footer' class='footer'></footer></div>"
         "<div id='short' class='column'><div id='grow'></div><div class='footer'></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("header")->height, 340.0f);
    EXPECT_FLOAT_EQ(element("main")->height, 1000.0f);
    EXPECT_FLOAT_EQ(element("footer")->height, 100.0f);
    EXPECT_FLOAT_EQ(element("tall")->height, 1440.0f);
    EXPECT_FLOAT_EQ(element("short")->height, 800.0f);
    EXPECT_FLOAT_EQ(element("grow")->height, 700.0f);
}

TEST_F(DynamicLayoutTest, ColumnFlexShrinksWithinAutomaticMaxHeight) {
    load("<!doctype html><style>div{display:flex;flex-direction:column}"
         "#parent{max-height:100px;width:100px}</style>"
         "<div id='parent'><div id='first' style='flex-basis:100px'></div>"
         "<div id='second' style='height:50px'></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_NEAR(element("first")->height, 200.0f / 3.0f, 0.1f);
    EXPECT_NEAR(element("second")->height, 100.0f / 3.0f, 0.1f);
}

TEST_F(DynamicLayoutTest, ColumnFlexZeroBasisDoesNotExpandToOverflowingContent) {
    load("<!doctype html><style>div{display:flex;flex-direction:column}</style>"
         "<div style='width:200px'><div id='item' "
         "style='overflow:hidden;padding:20px;flex-basis:0px'>"
         "<div style='height:200px;flex-shrink:0'></div></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("item")->height, 40.0f);
}

TEST_F(DynamicLayoutTest, NestedGridUsesAssignedStretchHeight) {
    load("<!doctype html><div style='display:grid;width:100px;height:100px'>"
         "<div style='display:grid;grid-template-rows:1fr 1fr'>"
         "<div id='first'></div><div id='second'></div></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("first")->height, 50.0f);
    EXPECT_FLOAT_EQ(element("second")->height, 50.0f);
}

TEST_F(DynamicLayoutTest, GridScrollBoundsUseFinalContentGrownRows) {
    load("<!doctype html><style>body{margin:0}#grid{display:grid;"
         "grid-template-columns:240px 1fr;grid-template-rows:1fr auto}"
         "#sidebar{grid-row:1/3;max-height:800px;overflow:auto}"
         "#content{overflow-x:auto}#content div{height:1400px}</style>"
         "<div id='grid'><aside id='sidebar'><div style='height:1200px'></div></aside>"
         "<main id='content'><div></div></main><footer>Footer</footer></div>");
    ASSERT_NE(document, nullptr);
    DomElement* content = element("content");
    DomElement* sidebar = element("sidebar");
    ASSERT_NE(content, nullptr);
    ASSERT_NE(sidebar, nullptr);
    ASSERT_NE(content->scroller, nullptr);
    EXPECT_FLOAT_EQ(content->height, 1400.0f);
    EXPECT_FALSE(content->scroll()->has_clip);
    Bound clip = {};
    ASSERT_TRUE(layout_block_overflow_clip(lam::view_require_block(sidebar), &clip));
    EXPECT_FLOAT_EQ(clip.bottom, sidebar->height);
    EXPECT_FLOAT_EQ(sidebar->height, 800.0f);
}

TEST_F(DynamicLayoutTest, GridPercentagePaddingUsesAreaWidth) {
    load("<!doctype html><div style='display:grid;width:200px'>"
         "<div id='item' style='display:flex;width:50%;padding:3%;box-sizing:border-box'>"
         "</div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("item")->width, 100.0f);
    EXPECT_FLOAT_EQ(element("item")->height, 12.0f);
}

TEST_F(DynamicLayoutTest, GridAbsoluteChildRetainsGridLineContainingBlock) {
    load("<!doctype html><div style='display:grid;position:absolute;"
         "grid-template-columns:40px 40px 40px;grid-template-rows:40px 40px 40px;"
         "padding:10px 20px 30px 40px'>"
         "<div id='item' style='position:absolute;grid-row-end:1;"
         "top:1px;bottom:2px;right:3px;left:4px'></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("item")->height, 7.0f);
    EXPECT_FLOAT_EQ(element("item")->y, 1.0f);
}

TEST_F(DynamicLayoutTest, OverflowingAtomicInlineFitsBeforeOutsideFloat) {
    load("<!doctype html><style>#wrap{width:600px;border:1px solid}"
         "#float{float:right;width:18%;height:160px}#narrow{width:80%}"
         "textarea{width:100%;height:100px}</style>"
         "<div id='wrap'><div id='float'></div><div id='narrow'>"
         "<textarea id='control'></textarea></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("control")->y, 0.0f);
}

TEST_F(DynamicLayoutTest, BoxlessElementsDoNotModifyCounters) {
    load("<!doctype html><style>#result:before{content:counter(x)}</style>"
         "<div><span style='counter-reset:x 6'></span>"
         "<span style='display:contents;counter-reset:x 666;counter-increment:x'></span>"
         "<span style='display:contents;counter-set:x 666'></span>"
         "<span id='result' style='counter-increment:x'></span></div>");
    ASSERT_NE(document, nullptr);
    ASSERT_NE(element("result")->pseudo, nullptr);
    DomElement* before = element("result")->pseudo->before;
    ASSERT_NE(before, nullptr);
    ASSERT_NE(before->first_child, nullptr);
    EXPECT_STREQ(before->first_child->as_text()->text, "7");
}

TEST_F(DynamicLayoutTest, TextBalanceLineBudgetExcludesBorder) {
    load("<!doctype html><div id='test' style='border:1px solid;"
         "font:15px/18px monospace;width:20ch;text-wrap:balance;overflow-wrap:anywhere'>"
         "Interconnect your build</div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("test")->height, 38.0f);
}

TEST_F(DynamicLayoutTest, EmptyTableCellDoesNotRaiseTextBaselineToSpecifiedHeight) {
    load("<!doctype html><style>table{border-collapse:collapse}tr{vertical-align:baseline}"
         "td{height:30px;border:1px solid}</style>"
         "<table><tr><td id='text'>ABCDE</td><td></td></tr></table>");
    ASSERT_NE(document, nullptr);
    ASSERT_NE(element("text")->first_child, nullptr);
    EXPECT_LT(element("text")->first_child->y, 5.0f);
}

TEST_F(DynamicLayoutTest, TableCellStaticPositionTracksMiddleAlignment) {
    load("<!doctype html><div id='cell' style='position:relative;display:table-cell;"
         "vertical-align:middle;width:100px;height:100px'>"
         "<div id='target' style='position:absolute;top:0;width:100px;height:100px'></div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("target")->y, 0.0f);
    ASSERT_TRUE(element("target")->set_attribute("style",
        "position:absolute;top:auto;width:100px;height:100px"));
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, true);
    EXPECT_FLOAT_EQ(element("target")->y, 50.0f);
}

TEST_F(DynamicLayoutTest, InlineBoundsIncludeDecoratedEmptyInline) {
    load("<!doctype html><span id='parent'>First line"
         "<span id='child' style='border:5px solid;border-right:none'></span></span>");
    ASSERT_NE(document, nullptr);
    EXPECT_LE(element("parent")->y, element("child")->y);
    EXPECT_GE(element("parent")->y + element("parent")->height,
              element("child")->y + element("child")->height);
}

TEST_F(DynamicLayoutTest, NegativeIndentDoesNotReappearAfterForcedBreak) {
    load("<!doctype html><body style='width:1px;font-size:12px'>"
         "<div id='box' style='float:left;height:24px;border:3px solid;text-indent:-36px'>"
         "<span style='display:inline-block;width:60px;height:12px'></span><br>"
         "<span style='display:inline-block;width:12px;height:12px'></span></div></body>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("box")->width, 30.0f);
}

TEST_F(DynamicLayoutTest, WhitespaceBetweenImageAndSvgIsPreserved) {
    load("<!doctype html><div style='font:16px Arial'>"
         "<img id='image' alt='Red box' style='width:80px;height:60px;display:inline-block'>\n "
         "<svg id='svg' style='width:80px;height:60px;display:inline-block;vertical-align:top' "
         "viewBox='0 0 80 60'><rect width='80' height='60'/></svg></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_GT(element("svg")->x - element("image")->x - element("image")->width, 1.0f);
}

TEST_F(DynamicLayoutTest, GridMaxContentTracksKeepFractionalWidth) {
    load("<!doctype html><div style='display:grid;width:400px;grid-template-columns:min-content max-content 1fr;gap:10px;font:14px Arial'>"
         "<div style='padding:10px;white-space:nowrap'>Min Content</div>"
         "<div id='content' style='padding:10px'>Maximum Content Width</div><div>Flexible</div></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_LT(element("content")->height, 40.0f);
}

TEST_F(DynamicLayoutTest, TableCellPercentagesDoNotGrowFirstPassRows) {
    load("<!doctype html><div style='display:flex;flex-direction:column'>"
         "<table id='table' style='border-spacing:0;width:100px;height:100px;flex:none'>"
         "<thead><tr style='height:50px'><td style='height:100%'></td></tr></thead>"
         "<tbody><tr style='height:50px'><td style='height:100%'></td></tr></tbody></table></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("table")->height, 100.0f);
}

TEST_F(DynamicLayoutTest, DefaultEmojiUsesColorFontLineMetrics) {
    load("<!doctype html><div style='font:16px Arial'>"
         "<span id='face' style='display:inline-block;padding:5px 10px'>😀</span>"
         "\n <span style='display:inline-block;padding:5px 10px'>❤️</span>"
         "\n <span id='star' style='display:inline-block;padding:5px 10px'>⭐</span>"
         "\n <span style='display:inline-block;padding:5px 10px'>🔥</span></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("star")->height, element("face")->height);
}

TEST_F(DynamicLayoutTest, BidiKeepsVisibleInteriorSpace) {
    load("<!doctype html><div style='font:32px/1 monospace'>"
         "<span dir='ltr'>x <span id='rtl' dir='rtl'> x </span> x</span></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_GT(element("rtl")->width, 30.0f);
}

TEST_F(DynamicLayoutTest, AutoWidthColumnsBalanceParagraphLines) {
    load("<!doctype html><div id='columns' style='width:1104px;column-width:200px;column-gap:25px;font:16px/1.6 Arial'>"
         "<p>This container will create as many columns as can fit based on the column-width setting. The actual number depends on the container width.</p>"
         "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed do eiusmod tempor incididunt ut labore et dolore magna aliqua.</p>"
         "<p>Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris.</p></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_NEAR(element("columns")->height, 92.8f, 1.0f);
}

TEST_F(DynamicLayoutTest, EmojiMetricsSurviveEarlierSymbolFallbacks) {
    char* source = read_text_file("test/layout/data/baseline/issue_unicode_width_001.html");
    ASSERT_NE(source, nullptr);
    load(source);
    free(source);
    ASSERT_NE(document, nullptr);
    DomNode* body = document->root->last_child;
    while (body && !body->is_element()) body = body->prev_sibling;
    ASSERT_NE(body, nullptr);
    DomNode* row = body->as_element()->last_child;
    while (row && !row->is_element()) row = row->prev_sibling;
    ASSERT_NE(row, nullptr);
    float emoji_height = 0.0f;
    for (DomNode* child = row->as_element()->first_child; child; child = child->next_sibling) {
        if (!child->is_element()) continue;
        if (emoji_height == 0.0f) emoji_height = child->height;
        EXPECT_FLOAT_EQ(child->height, emoji_height);
    }
}

TEST_F(DynamicLayoutTest, ReversedCounterIncludesFollowingSiblingScope) {
    load("<!doctype html><style>ol{counter-reset:reversed(foo)}"
         "li{counter-increment:foo -2}li::before{content:counters(foo,'.')}</style>"
         "<ol></ol><div><li id='first'></li><li></li><li id='last'></li></div>"
         "<div style='counter-reset:other'></div>"
         "<ol></ol><div><li id='again'></li><li></li><li></li></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_STREQ(element("first")->pseudo->before->first_child->as_text()->text, "6");
    EXPECT_STREQ(element("last")->pseudo->before->first_child->as_text()->text, "2");
    EXPECT_STREQ(element("again")->pseudo->before->first_child->as_text()->text, "6");
}

TEST_F(DynamicLayoutTest, ClearanceUsesFloatMarginBottom) {
    load("<!doctype html><body style='margin:0'><div style='height:164px'></div>"
         "<hr style='margin:0 0 24px'>"
         "<div style='float:left;width:100px;height:46px;margin-bottom:10px'></div>"
         "<hr id='clear' style='clear:both;margin:64px 0 24px'></body>");
    ASSERT_NE(document, nullptr);
    EXPECT_FLOAT_EQ(element("clear")->y, 246.0f);
}

TEST_F(DynamicLayoutTest, BalancedColumnsIncludeAdjoiningParagraphMargins) {
    load("<!doctype html><div id='columns' style='width:1104px;column-count:3;column-gap:30px;font:16px/1.6 Arial'>"
         "<p>Lorem ipsum dolor sit amet, consectetur adipiscing elit.</p>"
         "<p>Sed do eiusmod tempor incididunt ut labore et dolore.</p>"
         "<p>Ut enim ad minim veniam, quis nostrud exercitation.</p></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_NEAR(element("columns")->height, 72.53f, 1.0f);
}

TEST_F(DynamicLayoutTest, ReversedCounterIncludesOwnIncrement) {
    load("<!doctype html><style>span{counter-increment:part -2}"
         "#reset{counter-reset:reversed(part)}#result::before{content:counter(part)}</style>"
         "<div><span id='reset'></span><span></span><span id='result'></span>"
         "<span></span><span></span><span></span><span></span></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_STREQ(element("result")->pseudo->before->first_child->as_text()->text, "10");
}

TEST_F(DynamicLayoutTest, FlexSvgAutomaticCrossSizeUsesFlexedMainSize) {
    load("<!doctype html><style>body{margin:0}.page{display:flex;flex-direction:column}"
         ".bar{display:flex;align-items:center;padding:8px}.icon{width:14px}"
         "#logical{inline-size:14px}#padded{padding:2px;border:1px solid}"
         "#fixed{height:9px}</style><div class='page'>"
         "<a id='bar' class='bar'><span>Announcement</span>"
         "<svg id='physical' class='icon' viewBox='0 0 20 20'></svg>"
         "<svg id='logical' class='icon' viewBox='0 0 20 10'></svg>"
         "<svg id='padded' class='icon' viewBox='0 0 20 20'></svg>"
         "<svg id='fixed' class='icon' viewBox='0 0 20 20'></svg></a>"
         "<main id='hero'>Hero heading</main></div>");
    for (int phase = 0; phase < 2; phase++) {
        SCOPED_TRACE(phase);
        if (phase) {
            ASSERT_TRUE(element("physical")->set_attribute("style", "width:28px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        EXPECT_NEAR(element("physical")->width, phase ? 28.0f : 14.0f, 0.1f);
        EXPECT_NEAR(element("physical")->height, phase ? 28.0f : 14.0f, 0.1f);
        EXPECT_NEAR(element("logical")->width, 14.0f, 0.1f);
        EXPECT_NEAR(element("logical")->height, 7.0f, 0.1f);
        EXPECT_NEAR(element("padded")->width, 20.0f, 0.1f);
        EXPECT_NEAR(element("padded")->height, 20.0f, 0.1f);
        EXPECT_NEAR(element("fixed")->height, 9.0f, 0.1f);
        EXPECT_NEAR(element("bar")->height, phase ? 44.0f : 36.0f, 0.1f);
        EXPECT_NEAR(element("hero")->y, element("bar")->height, 0.1f);
    }
}

TEST_F(DynamicLayoutTest, PropagatedBodyBackgroundPrecedesNegativeStackingLayers) {
    const char* backgrounds[] = {"body{background:white}",
        "body{background:linear-gradient(white,blue)}", "html{background:lime}body{background:white}"};
    for (int phase = 0; phase < 3; phase++) {
        SCOPED_TRACE(phase);
        if (document) { free_document(document); document = nullptr; ui.document = nullptr; }
        StrBuf* source = strbuf_create("<!doctype html><style>html,body{margin:0}");
        strbuf_append_str(source, backgrounds[phase]);
        strbuf_append_str(source, ".hero{position:relative;height:800px}"
            ".sky{position:absolute;inset:0 auto 0 0;width:100px;z-index:-1;background:red}</style>"
            "<div class='hero'><div class='sky'></div>Heading</div>");
        load(source->str);
        strbuf_free(source);
        render_html_doc(&ui, document->view_tree, "temp/ledger-canvas-order.png");
        ASSERT_FALSE(ui.render_failed);
        int width, height, channels;
        unsigned char* pixels = image_load("temp/ledger-canvas-order.png", &width, &height, &channels, 4);
        ASSERT_NE(pixels, nullptr);
        ASSERT_GT(width, 100); ASSERT_GT(height, 100);
        const unsigned char* pixel = pixels + (100 * width + 50) * 4;
        EXPECT_EQ(pixel[0], 255);
        EXPECT_EQ(pixel[1], phase == 2 ? 255 : 0);
        EXPECT_EQ(pixel[2], phase == 2 ? 255 : 0);
        const unsigned char* canvas = pixels + (100 * width + 200) * 4;
        EXPECT_EQ(canvas[2], 255);
        if (phase == 1) EXPECT_LT(canvas[0], 250);
        else EXPECT_EQ(canvas[0], 255);
        image_free(pixels);
    }
}

TEST_F(DynamicLayoutTest, PositionedIntrinsicSizingDoesNotLeakIntoFollowingBlocks) {
    load("<!doctype html><style>body{margin:0}header{position:fixed;width:max-content}"
         "</style><header id='navigation'>Navigation</header>"
         "<main id='flow'><div style='width:3000px;height:20px'>Wide content</div></main>"
         "<div id='next'>Following sibling</div>");
    const char* styles[] = {"position:fixed;width:max-content", "position:absolute;width:min-content",
        "position:fixed;width:140px"};
    for (const char* style : styles) {
        SCOPED_TRACE(style);
        ASSERT_TRUE(element("navigation")->set_attribute("style", style));
        radiant_recascade_document(document);
        layout_html_doc(&ui, document, false);
        EXPECT_NEAR(element("flow")->width, 1200.0f, 0.1f);
        EXPECT_NEAR(element("next")->width, 1200.0f, 0.1f);
        EXPECT_NEAR(element("flow")->first_child->width, 3000.0f, 0.1f);
    }
}

TEST_F(DynamicLayoutTest, NestedColumnIntrinsicHeightIncludesMainAxisGaps) {
    load("<!doctype html><style>html{height:100%}body{margin:0;display:flex;"
         "flex-direction:column;min-height:100vh}main,.page{display:flex;"
         "flex-direction:column;flex-grow:1}.page{gap:120px;padding:20px}"
         ".hero{display:flex;flex-direction:column;height:92vh;min-height:540px;max-height:1100px}"
         ".section{height:100%;display:flex;flex-direction:column;flex-grow:1;overflow:hidden}"
         ".picture{height:100%;min-height:600px}.rest{height:6000px}</style>"
         "<div><main id='main'><div id='page' class='page'><div id='hero' class='hero'>"
         "<section class='section'><div class='picture'>Hero</div></section></div>"
         "<div id='rest' class='rest'>Rest</div></div></main><footer style='height:400px'>Footer</footer></div>");
    for (float gap : {120.0f, 40.0f}) {
        if (gap == 40.0f) {
            ASSERT_TRUE(element("page")->set_attribute("style", "gap:40px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        EXPECT_NEAR(element("hero")->height, 736.0f, 0.1f);
        EXPECT_NEAR(element("rest")->height, 6000.0f, 0.1f);
        EXPECT_NEAR(element("page")->height, 40.0f + 736.0f + gap + 6000.0f, 0.1f);
        EXPECT_NEAR(element("main")->height, element("page")->height, 0.1f);
    }
}

TEST_F(DynamicLayoutTest, ScriptingEnabledNoscriptCannotCreateBoxesThroughCssDisplay) {
    load("<!doctype html><style>body{margin:0}p{margin:0;height:20px}"
         "noscript{height:400px}#contents{display:contents}.flex{display:flex;gap:50px}"
         ".grid{display:grid;gap:50px}</style>"
         "<div><noscript id='contents'><span>Fallback contents</span></noscript><p id='one'>One</p></div>"
         "<div id='flex-parent' class='flex'><noscript id='flexed' style='display:block'>Fallback flex</noscript><p id='two'>Two</p></div>"
         "<div id='grid-parent' class='grid'><noscript id='gridded' style='display:block'>Fallback grid</noscript><p id='three'>Three</p></div>");
    ASSERT_NE(document, nullptr);
    EXPECT_FALSE(document->html_scripting_enabled);
    EXPECT_GT(element("flexed")->height, 0.0f);
    document->html_scripting_enabled = true;
    radiant_recascade_document(document);
    layout_html_doc(&ui, document, false);
    for (const char* id : {"contents", "flexed", "gridded"}) {
        SCOPED_TRACE(id);
        EXPECT_EQ(element(id)->view_type, RDT_VIEW_NONE);
        EXPECT_NE(resolve_display_value(element(id)).outer, CSS_VALUE_NONE);
        ASSERT_NE(element(id)->first_child, nullptr);
    }
    EXPECT_FLOAT_EQ(element("one")->y, 0.0f);
    EXPECT_FLOAT_EQ(element("two")->y, 0.0f);
    EXPECT_FLOAT_EQ(element("three")->y, 0.0f);
    EXPECT_FLOAT_EQ(element("flex-parent")->y, 20.0f);
    EXPECT_FLOAT_EQ(element("grid-parent")->y, 40.0f);
}

TEST_F(DynamicLayoutTest, GridRatioHeightUsesConstrainedInlineSize) {
    load("<!doctype html><style>body{margin:0}.grid{display:grid;width:856px;"
         "grid-template-columns:repeat(4,minmax(0,1fr));place-items:center;gap:12px}"
         ".tile{display:flex;aspect-ratio:1;width:100%;max-width:56px}"
         "img{width:100%;height:100%}</style>"
         "<div id='image-grid' class='grid'><a id='image-tile' class='tile'>"
         "<img id='image' width='48' height='48'></a></div>"
         "<div id='empty-grid' class='grid'><a id='empty-tile' class='tile'></a></div>"
         "<div id='fixed-grid' class='grid'><a id='fixed-tile' class='tile' style='height:90px'></a></div>");
    for (float limit : {56.0f, 112.0f}) {
        if (limit == 112.0f) {
            ASSERT_TRUE(element("image-tile")->set_attribute("style", "max-width:112px"));
            ASSERT_TRUE(element("empty-tile")->set_attribute("style", "max-width:112px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        EXPECT_FLOAT_EQ(element("image-grid")->height, limit);
        EXPECT_FLOAT_EQ(element("empty-grid")->height, limit);
        EXPECT_FLOAT_EQ(element("image-tile")->width, limit);
        EXPECT_FLOAT_EQ(element("image-tile")->height, limit);
        EXPECT_FLOAT_EQ(element("image")->width, limit);
        EXPECT_FLOAT_EQ(element("image")->height, limit);
        EXPECT_FLOAT_EQ(element("fixed-grid")->height, 90.0f);
        EXPECT_FLOAT_EQ(element("fixed-tile")->height, 90.0f);
    }
}

TEST_F(DynamicLayoutTest, IntrinsicDescendantPercentHeightUsesItsOwnContainingBlock) {
    load("<!doctype html><style>*{box-sizing:border-box}html{height:100%}"
         "body{margin:0;display:flex;flex-direction:column;min-height:100vh}"
         "main,.page,.rest{display:flex;flex-direction:column;flex-grow:1}"
         ".page{gap:120px;padding:20px}.hero{height:92vh;min-height:540px;max-height:1100px}"
         ".rest{margin:auto;width:100%;max-width:1080px}.section{margin:60px 0}"
         ".card{display:flex;flex-direction:column;overflow:hidden;padding:40px}"
         ".grid{display:grid;grid-template-columns:repeat(4,minmax(0,1fr));place-items:center;gap:12px}"
         ".tile{display:flex;aspect-ratio:1;width:100%;max-width:56px}img{width:100%;height:100%}"
         "</style><div><main><div id='page' class='page'><div id='hero' class='hero'>Hero</div>"
         "<div id='rest' class='rest'><div id='section' class='section'><div class='card'>"
         "<div class='grid'><a id='tile' class='tile'><img id='image' width='48' height='48'></a></div>"
         "</div></div></div></div></main><footer style='height:400px'></footer></div>");
    for (float limit : {56.0f, 112.0f}) {
        if (limit == 112.0f) {
            ASSERT_TRUE(element("tile")->set_attribute("style", "max-width:112px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        EXPECT_FLOAT_EQ(element("hero")->height, 736.0f);
        EXPECT_FLOAT_EQ(element("section")->height, limit + 80.0f);
        EXPECT_FLOAT_EQ(element("rest")->height, limit + 200.0f);
        EXPECT_FLOAT_EQ(element("page")->height, limit + 1096.0f);
        EXPECT_FLOAT_EQ(element("image")->height, limit);
    }
}

TEST_F(DynamicLayoutTest, MaximumConstraintPreservesDefiniteFlexMainAxis) {
    load("<!doctype html><style>*{box-sizing:border-box}body{margin:0}"
         "#column{height:736px;width:900px;max-height:1100px;display:flex;flex-direction:column}"
         "#fill,#inner,#overlay{height:100%}#inner{min-height:600px}"
         "#overlay{padding-top:120px;position:relative;display:flex}"
         "#marker{position:absolute;bottom:0;width:100px;height:100px}"
         "#row{display:flex;width:736px;max-width:1100px}#half{flex:0 0 50%;height:10px}"
         "</style><div id='column'><div id='fill'><div id='inner'>"
         "<div id='overlay'><div id='marker'></div></div></div></div></div>"
         "<div id='row'><div id='half'></div></div>");
    const char* height_limits[] = {"max-height:1100px", "max-height:600px", "max-height:none"};
    const char* width_limits[] = {"max-width:1100px", "max-width:600px", "max-width:none"};
    const float sizes[] = {736.0f, 600.0f, 736.0f};
    for (int phase = 0; phase < 3; phase++) {
        SCOPED_TRACE(phase);
        if (phase) {
            ASSERT_TRUE(element("column")->set_attribute("style", height_limits[phase]));
            ASSERT_TRUE(element("row")->set_attribute("style", width_limits[phase]));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        EXPECT_FLOAT_EQ(element("column")->height, sizes[phase]);
        EXPECT_FLOAT_EQ(element("fill")->height, sizes[phase]);
        EXPECT_FLOAT_EQ(element("inner")->height, sizes[phase]);
        EXPECT_FLOAT_EQ(element("overlay")->height, sizes[phase]);
        EXPECT_FLOAT_EQ(element("marker")->y, sizes[phase] - 100.0f);
        EXPECT_FLOAT_EQ(element("row")->width, sizes[phase]);
        EXPECT_FLOAT_EQ(element("half")->width, sizes[phase] / 2.0f);
    }
}

TEST_F(DynamicLayoutTest, MathGapsUseOwnerUnitsAcrossLayoutModes) {
    load("<!doctype html><style>:root{font-size:16px;--spacing:.25rem}body{margin:0}"
         ".item{width:10px;height:10px}#row,#column,#em,#percent,#negative{display:flex}"
         "#row{gap:calc(var(--spacing) * 10)}"
         "#column{flex-direction:column;row-gap:max(8px,1rem)}"
         "#grid{display:grid;grid-template-columns:10px 10px;"
         "gap:clamp(8px,1rem,30px) calc(3px * 4)}"
         "#em{font-size:10px;column-gap:calc(2 * 1em)}"
         "#percent{width:400px;column-gap:10%}"
         "#negative{gap:calc(4px - 10px)}"
         "#multicol{width:240px;height:40px;column-count:2;column-fill:auto;"
         "column-gap:calc(2px * 10)}#multicol>div{height:20px}</style>"
         "<div id='row'><div class='item'></div><div id='row-next' class='item'></div></div>"
         "<div id='column'><div class='item'></div><div id='column-next' class='item'></div></div>"
         "<div id='grid'><div class='item'></div><div id='grid-next' class='item'></div>"
         "<div id='grid-row' class='item'></div></div>"
         "<div id='em'><div class='item'></div><div id='em-next' class='item'></div></div>"
         "<div id='percent'><div class='item'></div><div id='percent-next' class='item'></div></div>"
         "<div id='negative'><div class='item'></div><div id='negative-next' class='item'></div></div>"
         "<div id='multicol'><div></div><div></div><div id='column-two'></div><div></div></div>");
    for (int phase = 0; phase < 2; phase++) {
        SCOPED_TRACE(phase);
        if (phase) {
            ASSERT_TRUE(document->root->set_attribute("style", "font-size:20px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        EXPECT_FLOAT_EQ(element("row-next")->x, phase ? 60.0f : 50.0f);
        EXPECT_FLOAT_EQ(element("column-next")->y, phase ? 30.0f : 26.0f);
        EXPECT_FLOAT_EQ(element("grid-next")->x, 22.0f);
        EXPECT_FLOAT_EQ(element("grid-row")->y, phase ? 30.0f : 26.0f);
        EXPECT_FLOAT_EQ(element("em-next")->x, 30.0f);
        EXPECT_FLOAT_EQ(element("percent-next")->x, 50.0f);
        EXPECT_FLOAT_EQ(element("negative-next")->x, 10.0f);
        EXPECT_FLOAT_EQ(element("column-two")->x, 130.0f);
    }
}

TEST_F(DynamicLayoutTest, IntrinsicVariableMaximumConstrainsNestedFlexContributions) {
    StrBuf* source = strbuf_create("<!doctype html><style>body{margin:0}"
        ".row{display:flex;width:400px;gap:20px}.column{display:flex;flex-direction:column}"
        ".heading{width:100px;height:10px}.copy{margin:0;font:10px/10px monospace}"
        ".picture{flex:0 1 200px;min-width:0;height:10px}"
        "#literal .copy{max-width:300px}"
        "#variable .copy{--cap:300px;max-width:var(--cap)}"
        "#font .copy{--cap:30em;max-width:var(--cap)}"
        "#function .copy{--cap:20em;max-width:calc(var(--cap) + 100px)}"
        "#math .copy{--cap:30em;max-width:calc(var(--cap) + 0%)}"
        "</style>");
    const char* variants[] = {"literal", "variable", "font", "function", "math", "unconstrained"};
    for (const char* variant : variants) {
        strbuf_append_format(source, "<div id='%s' class='row'><div id='%s-column' class='column'>"
            "<div class='heading'></div><p id='%s-copy' class='copy'>", variant, variant, variant);
        for (int word = 0; word < 30; word++) strbuf_append_str(source, "content ");
        strbuf_append_format(source, "</p></div><div id='%s-picture' class='picture'></div></div>", variant);
    }
    load(source->str);
    strbuf_free(source);
    for (int phase = 0; phase < 2; phase++) {
        if (phase) {
            ASSERT_TRUE(element("literal-copy")->set_attribute("style", "max-width:200px"));
            ASSERT_TRUE(element("variable-copy")->set_attribute("style", "--cap:200px"));
            ASSERT_TRUE(element("font-copy")->set_attribute("style", "--cap:20em"));
            ASSERT_TRUE(element("function-copy")->set_attribute("style", "--cap:10em"));
            ASSERT_TRUE(element("math-copy")->set_attribute("style", "--cap:20em"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        for (const char* variant : variants) {
            SCOPED_TRACE(variant);
            SCOPED_TRACE(phase);
            if (strcmp(variant, "unconstrained") == 0) continue;
            char column_id[32], picture_id[32];
            snprintf(column_id, sizeof(column_id), "%s-column", variant);
            snprintf(picture_id, sizeof(picture_id), "%s-picture", variant);
            bool cyclic = strcmp(variant, "math") == 0;
            EXPECT_NEAR(element(column_id)->width, cyclic ? element("unconstrained-column")->width
                : phase ? 190.0f : 228.0f, 0.1f);
            EXPECT_NEAR(element(picture_id)->width, cyclic ? element("unconstrained-picture")->width
                : phase ? 190.0f : 152.0f, 0.1f);
        }
    }
}

TEST_F(DynamicLayoutTest, IntrinsicImageCssOverrideDoesNotRestoreHtmlWidthHint) {
    ASSERT_EQ(write_text_file_atomic("temp/intrinsic-natural.svg",
        "<svg xmlns='http://www.w3.org/2000/svg' width='300' height='100'>"
        "<rect width='300' height='100' fill='red'/></svg>"), 0);
    StrBuf* source = strbuf_create("<!doctype html><style>body{margin:0}"
        ".row{display:flex;width:400px}.text{flex:0 0 200px}.picture{min-width:0}"
        "img{height:auto}#percent img{width:100%}#automatic img{width:auto}"
        "#variable img{--image-width:100%;width:var(--image-width)}"
        "#logical img{inline-size:100%}#fixed img{width:150px}</style>");
    const char* variants[] = {"percent", "automatic", "variable", "logical", "hint", "fixed"};
    for (const char* variant : variants) {
        strbuf_append_format(source, "<div id='%s' class='row'><div class='text'></div>"
            "<div id='%s-picture' class='picture'><img id='%s-image' "
            "src='intrinsic-natural.svg' width='100' height='100'></div></div>",
            variant, variant, variant);
    }
    load(source->str);
    strbuf_free(source);
    for (int phase = 0; phase < 2; phase++) {
        if (phase) {
            for (const char* variant : variants) {
                char image_id[32];
                snprintf(image_id, sizeof(image_id), "%s-image", variant);
                ASSERT_TRUE(element(variant)->set_attribute("style", "width:500px"));
                ASSERT_TRUE(element(image_id)->set_attribute("width", "80"));
            }
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        for (const char* variant : variants) {
            SCOPED_TRACE(variant);
            SCOPED_TRACE(phase);
            float expected = strcmp(variant, "hint") == 0 ? (phase ? 80.0f : 100.0f)
                : strcmp(variant, "fixed") == 0 ? 150.0f : phase ? 300.0f : 200.0f;
            char picture_id[32], image_id[32];
            snprintf(picture_id, sizeof(picture_id), "%s-picture", variant);
            snprintf(image_id, sizeof(image_id), "%s-image", variant);
            EXPECT_NEAR(element(picture_id)->width, expected, 0.1f);
            float image_width = strcmp(variant, "automatic") == 0 ? 300.0f : expected;
            EXPECT_NEAR(element(image_id)->width, image_width, 0.1f);
            EXPECT_NEAR(element(image_id)->height, image_width / 3.0f, 0.1f);
        }
    }
}

TEST(UiSessionTest, OwnedEphemeralBrowsingSessionEndsWithUi) {
    UiContext ui = {};
    ASSERT_EQ(ui_context_init(&ui, true, 1.0f), 0);
    ASSERT_TRUE(ui.ensure_browsing_session());
    CookieJar* jar = session_cookie_jar(ui.browsing_session);
    ASSERT_NE(jar, nullptr);
    BrowsingSession* initial = ui.browsing_session;
    ASSERT_TRUE(ui.ensure_browsing_session());
    EXPECT_EQ(ui.browsing_session, initial);
    cookie_jar_store(jar, "https://session.test/", "token=kept; Path=/");
    char* value = cookie_jar_build_document_cookie(jar, "https://session.test/");
    EXPECT_STREQ(value, "token=kept");
    mem_free(value);
    ASSERT_TRUE(ui_context_cleanup(&ui));
    EXPECT_EQ(ui.browsing_session, nullptr);
    EXPECT_FALSE(ui.owned_browsing_session);

    ASSERT_EQ(ui_context_init(&ui, true, 1.0f), 0);
    ASSERT_TRUE(ui.ensure_browsing_session());
    jar = session_cookie_jar(ui.browsing_session);
    EXPECT_EQ(cookie_jar_count(jar), 0);
    value = cookie_jar_build_document_cookie(jar, "https://session.test/");
    EXPECT_EQ(value, nullptr);
    mem_free(value);
    EXPECT_TRUE(ui_context_cleanup(&ui));
}

TEST(UiSessionTest, BorrowedBrowsingSessionSurvivesUiCleanup) {
    UiContext ui = {};
    ASSERT_EQ(ui_context_init(&ui, true, 1.0f), 0);
    BrowsingSession* borrowed = session_create(nullptr, nullptr);
    ASSERT_NE(borrowed, nullptr);
    ui.browsing_session = borrowed;
    ASSERT_TRUE(ui.ensure_browsing_session());
    EXPECT_EQ(ui.browsing_session, borrowed);
    EXPECT_FALSE(ui.owned_browsing_session);
    cookie_jar_store(session_cookie_jar(borrowed), "https://session.test/", "borrowed=alive; Path=/");
    ASSERT_TRUE(ui_context_cleanup(&ui));
    EXPECT_EQ(ui.browsing_session, borrowed);
    char* value = cookie_jar_build_document_cookie(session_cookie_jar(borrowed),
        "https://session.test/");
    EXPECT_STREQ(value, "borrowed=alive");
    mem_free(value);
    ui.browsing_session = nullptr;
    session_destroy(borrowed);
}

TEST_F(DynamicLayoutTest, GridPercentageContributionsUseTheUnsolvedArea) {
    const struct {
        const char* name;
        const char* style;
        float fraction;
        float fixed_width;
    } variants[] = {
        {"full", "width:100%", 1.0f, -1.0f},
        {"half", "width:50%", 0.5f, -1.0f},
        {"math", "width:calc(100% - 0px)", 1.0f, -1.0f},
        {"variable", "--fill:100%;width:var(--fill)", 1.0f, -1.0f},
        {"automatic", "width:auto", 1.0f, -1.0f},
        {"minimum", "width:100%;min-width:300px", 1.0f, -1.0f},
        {"maximum", "width:100%;max-width:300px", 1.0f, 300.0f},
        {"percent-max", "width:auto;max-width:50%", 0.5f, -1.0f},
        {"fixed", "width:600px", 1.0f, 600.0f}
    };
    StrBuf* source = strbuf_create("<!doctype html><style>body{margin:0}"
        ".grid{display:grid;width:var(--grid-width,1160px);grid-template-columns:1fr 1fr;gap:64px}"
        ".column{display:flex;flex-direction:column;align-items:flex-start}"
        ".copy{width:160px;height:20px}.picture{width:560px;height:80px;margin-right:-180px}"
        "</style>");
    for (const auto& variant : variants) {
        strbuf_append_format(source, "<div class='grid'><div id='%s' class='column' style='%s'>"
            "<div class='copy'></div></div><div id='%s-peer'><div class='picture'></div></div></div>",
            variant.name, variant.style, variant.name);
    }
    load(source->str);
    strbuf_free(source);
    for (int phase = 0; phase < 2; phase++) {
        if (phase) {
            ASSERT_TRUE(document->root->set_attribute("style", "--grid-width:1080px"));
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
        }
        float track = phase ? 508.0f : 548.0f;
        for (const auto& variant : variants) {
            SCOPED_TRACE(variant.name);
            SCOPED_TRACE(phase);
            char peer_id[32];
            snprintf(peer_id, sizeof(peer_id), "%s-peer", variant.name);
            float expected = variant.fixed_width >= 0.0f ? variant.fixed_width
                : track * variant.fraction;
            EXPECT_FLOAT_EQ(element(variant.name)->width, expected);
            EXPECT_FLOAT_EQ(element(peer_id)->x,
                strcmp(variant.name, "fixed") == 0 ? 664.0f : track + 64.0f);
        }
    }
}

TEST_F(DynamicLayoutTest, RootRelativeFontSizesPublishTheComputedRemBasis) {
    load("<!doctype html><style>html{font-size:clamp(.875rem,.25rem + .8333vw,1rem);"
         "padding:1rem;box-sizing:border-box}body{margin:0}"
         "#box{width:10rem;height:2rem}#math{width:calc(2rem + 10px);height:1rem}"
         "#label{font:4rem/1 Arial;margin:0;white-space:nowrap}</style>"
         "<body id='body'><div id='box'></div><div id='math'></div><h1 id='label'>root units</h1></body>");
    const struct {
        const char* style;
        float font_size;
        float scale;
        float viewport;
    } variants[] = {
        {"", 14.0f, 1.0f, 1200.0f},
        {"font-size:.75rem", 12.0f, 1.0f, 1200.0f},
        {"font-size:2rem", 32.0f, 1.0f, 1200.0f},
        {"--font:.875rem;font-size:var(--font)", 14.0f, 1.0f, 1200.0f},
        {"font-size:calc(.5rem + 6px)", 14.0f, 1.0f, 1200.0f},
        {"font:.875rem/1.5 Arial", 14.0f, 1.0f, 1200.0f},
        {"font-size:14px", 14.0f, 1.0f, 1200.0f},
        {"font-size:.875rem;zoom:2", 14.0f, 2.0f, 1200.0f},
        {"", 16.0f, 1.0f, 1600.0f}
    };
    for (const auto& variant : variants) {
        SCOPED_TRACE(variant.style);
        ASSERT_TRUE(document->root->set_attribute("style", variant.style));
        ui.viewport_width = variant.viewport;
        for (int repeat = 0; repeat < 2; repeat++) {
            SCOPED_TRACE(repeat);
            radiant_recascade_document(document);
            layout_html_doc(&ui, document, false);
            ASSERT_NE(document->root->font, nullptr);
            ASSERT_NE(element("label")->font, nullptr);
            EXPECT_FLOAT_EQ(document->root->fontp()->font_size, variant.font_size);
            EXPECT_FLOAT_EQ(element("label")->fontp()->font_size, variant.font_size * 4.0f);
            EXPECT_FLOAT_EQ(element("box")->width, variant.font_size * variant.scale * 10.0f);
            EXPECT_FLOAT_EQ(element("box")->height, variant.font_size * variant.scale * 2.0f);
            EXPECT_FLOAT_EQ(document->root->boundary()->padding.left, variant.font_size * variant.scale);
            EXPECT_FLOAT_EQ(element("math")->width, (variant.font_size * 2.0f + 10.0f) * variant.scale);
            EXPECT_FLOAT_EQ(element("label")->height, variant.font_size * variant.scale * 4.0f);
        }
    }
}
