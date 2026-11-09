#include <gtest/gtest.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "radiant/loader_runtime_host.hpp"
#include "radiant/view.hpp"
#include "radiant/radiant.hpp"
#include "lambda/input/input.hpp"
#include "lib/file.h"
#include "lib/mem.h"
#include "lib/url.h"

static bool is_layout_fixture(const char* name) {
    const char* extension = strrchr(name, '.');
    return extension && (strcasecmp(extension, ".html") == 0 ||
        strcasecmp(extension, ".htm") == 0 || strcasecmp(extension, ".xht") == 0 ||
        strcasecmp(extension, ".xhtml") == 0);
}

static bool skip_fixture_directory(const char* name) {
    return name[0] == '.' || strcmp(name, "support") == 0 || strcmp(name, "tools") == 0;
}

static void load_layout_fixtures(const char* directory, UiContext* ui,
                                 const DocumentJsHostConfig* host,
                                 size_t* fixture_count, size_t* laid_out_count) {
    DIR* dir = opendir(directory);
    if (!dir) {
        ADD_FAILURE() << "could not open fixture directory " << directory;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_name[0] == '.' &&
            (entry->d_name[1] == '\0' ||
             (entry->d_name[1] == '.' && entry->d_name[2] == '\0'))) continue;

        char fixture_path[PATH_MAX];
        int path_length = snprintf(fixture_path, sizeof(fixture_path), "%s/%s",
                                  directory, entry->d_name);
        if (path_length < 0 || (size_t)path_length >= sizeof(fixture_path)) {
            ADD_FAILURE() << "fixture path is too long in " << directory;
            continue;
        }

        struct stat info;
        if (stat(fixture_path, &info) != 0) {
            ADD_FAILURE() << "could not stat fixture " << fixture_path;
            continue;
        }
        if (S_ISDIR(info.st_mode)) {
            if (!skip_fixture_directory(entry->d_name))
                load_layout_fixtures(fixture_path, ui, host, fixture_count, laid_out_count);
            continue;
        }
        if (!S_ISREG(info.st_mode) || !is_layout_fixture(entry->d_name)) continue;

        (*fixture_count)++;
        SCOPED_TRACE(fixture_path);
        char* absolute_path = file_realpath(fixture_path);
        if (!absolute_path) {
            ADD_FAILURE() << "could not resolve fixture path";
            continue;
        }
        char* document_url = url_from_local_path(absolute_path);
        mem_free(absolute_path);
        if (!document_url) {
            ADD_FAILURE() << "could not build fixture URL";
            continue;
        }

        DomDocument* document = load_html_doc(nullptr, document_url, 1200, 800, host);
        mem_free(document_url);
        if (!document) {
            ADD_FAILURE() << "native loader could not create a document";
            continue;
        }
        EXPECT_TRUE(document->scripting_disabled);
        EXPECT_FALSE(document->html_scripting_enabled);
        ui->document = lam::up(document);
        layout_html_doc(ui, document, false);
        if (!document->view_tree || !document->view_tree->root) {
            ADD_FAILURE() << "native layout did not produce a view tree";
        } else {
            (*laid_out_count)++;
        }
        ui->document = nullptr;
        free_document(document);
    }
    closedir(dir);
}

TEST(RadiantLayoutBaseline, LoadsEveryNonDynamicHtmlFixture) {
    ASSERT_FALSE(radiant_loader_prepare_script());
    UiContext ui = {};
    ASSERT_EQ(ui_context_init(&ui, true, 1.0f), 0);
    ui.viewport_width = 1200;
    ui.viewport_height = 800;

    DocumentJsHostConfig host = {};
    host.ui_context = &ui;
    host.resource_policy = INPUT_RESOURCE_LOCAL_ONLY;

    size_t fixture_count = 0;
    size_t laid_out_count = 0;
    load_layout_fixtures("test/layout/data/baseline", &ui, &host,
                         &fixture_count, &laid_out_count);
    EXPECT_GT(fixture_count, 0u);
    EXPECT_EQ(laid_out_count, fixture_count);
    ::testing::Test::RecordProperty("layout_fixture_count", (int)fixture_count);

    EXPECT_TRUE(ui_context_cleanup(&ui));
    InputManager::destroy_global();
}
