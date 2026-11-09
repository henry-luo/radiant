#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/file.h"
#include "lib/host_paths.h"
#include "lib/log.h"
#include "lib/mem_context.h"
#include "lib/uv_loop.h"
#include "lambda/input/input.hpp"
#include "lambda/input/input-postscript.hpp"
#include "lambda/input/css/css_style.hpp"
#include "lib/document_transform.h"
#include "radiant/loader_runtime_host.hpp"
#include "radiant/clipboard.hpp"
#include "radiant/event.hpp"
#include "radiant/render.hpp"
#include "radiant/window_host.hpp"

// The executable installs the scripting provider; libradiant itself remains
// independent of the runtime that owns that provider.
bool radiant_register_dom_engine_providers();
void lambda_home_init();
void module_ast_prebuild_cleanup();
extern int g_js_force_document_interp;
int cmd_layout(int argc, char** argv);

static void usage(FILE* stream) {
    fputs("Usage: radiant.exe layout <document> [options]\n"
          "       radiant.exe render <document> -o <output> [options]\n"
          "       radiant.exe render-batch [options]\n"
          "Use 'radiant.exe layout --help' or 'radiant.exe render --help' for options.\n", stream);
}

static bool parse_viewport(const char* text, int* result) {
    char* end = nullptr;
    long value = strtol(text, &end, 10);
    if (!*text || *end || value <= 0 || value > INT_MAX) return false;
    *result = (int)value; // INT_CAST_OK: validated CLI viewport integer.
    return true;
}

static int command_render(int argc, char** argv) {
    const char* input = nullptr;
    const char* output = nullptr;
    const char* theme = nullptr;
    const char* view_key = nullptr;
    int width = 0, height = 0;
    float scale = 1.0f, device_scale = 1.0f;
    bool paged = false, paged_options_given = false;
    RenderPagedOptions paged_options = render_paged_options_default();
    for (int i = 0; i < argc; i++) {
        const char* arg = argv[i];
        if (!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
            fputs("Usage: radiant.exe render <document> -o <output.svg|pdf|png|jpg> [options]\n"
                  "  -vw, --viewport-width N   Viewport width\n"
                  "  -vh, --viewport-height N  Viewport height\n"
                  "  -s, --scale N             Export scale\n"
                  "  --pixel-ratio N           Device scale\n"
                  "  --paged                   Enable paged output and preview options\n"
                  "  --pages, --export-pages   Select preview or exported pages\n"
                  "  --thumbnail-page N        Render one page as an image\n"
                  "  --theme NAME              Graph rendering theme\n"
                  "  --view-key KEY            Structurizr view key\n", stdout);
            return 0;
        }
        if (!strcmp(arg, "--paged")) { paged = true; continue; }
        if (!strcmp(arg, "-o") || !strcmp(arg, "--output") ||
            !strcmp(arg, "-vw") || !strcmp(arg, "--viewport-width") ||
            !strcmp(arg, "-vh") || !strcmp(arg, "--viewport-height") ||
            !strcmp(arg, "-s") || !strcmp(arg, "--scale") || !strcmp(arg, "--pixel-ratio") ||
            !strcmp(arg, "-t") || !strcmp(arg, "--theme") || !strcmp(arg, "--view-key")) {
            if (++i >= argc) { fprintf(stderr, "Missing value for %s\n", arg); return 1; }
            const char* value = argv[i];
            if (!strcmp(arg, "-o") || !strcmp(arg, "--output")) output = value;
            else if (!strcmp(arg, "-vw") || !strcmp(arg, "--viewport-width")) {
                if (!parse_viewport(value, &width)) { fprintf(stderr, "Invalid viewport width: %s\n", value); return 1; }
            } else if (!strcmp(arg, "-vh") || !strcmp(arg, "--viewport-height")) {
                if (!parse_viewport(value, &height)) { fprintf(stderr, "Invalid viewport height: %s\n", value); return 1; }
            } else if (!strcmp(arg, "-t") || !strcmp(arg, "--theme")) theme = value;
            else if (!strcmp(arg, "--view-key")) view_key = value;
            else if (!render_output_parse_extent(value,
                    !strcmp(arg, "--pixel-ratio") ? &device_scale : &scale)) {
                fprintf(stderr, "Invalid scale: %s\n", value); return 1;
            }
            continue;
        }
        int arity = render_paged_option_arity(arg);
        if (arity >= 0) {
            const char* value = nullptr;
            if (arity && ++i >= argc) { fprintf(stderr, "Missing value for %s\n", arg); return 1; }
            if (arity) value = argv[i];
            const char* error = nullptr;
            if (!render_paged_option_apply(&paged_options, arg, value, &error)) {
                fprintf(stderr, "%s: %s\n", arg, error ? error : "invalid option"); return 1;
            }
            paged_options_given = true;
            continue;
        }
        if (arg[0] == '-' || input) { fprintf(stderr, "Unexpected argument: %s\n", arg); return 1; }
        input = arg;
    }
    if (!input || !output) { usage(stderr); return 1; }
    if (paged_options_given && !paged) { fputs("Paged options require --paged\n", stderr); return 1; }
    if (paged && !render_paged_input_supported(input)) { fputs("Unsupported paged input\n", stderr); return 1; }
    const char* ext = file_path_ext(output);
    if (!ext || (strcmp(ext, ".png") && strcmp(ext, ".jpg") && strcmp(ext, ".jpeg") &&
                 strcmp(ext, ".svg") && strcmp(ext, ".pdf"))) {
        fputs("Output must use .png, .jpg, .jpeg, .svg, or .pdf\n", stderr); return 1;
    }
    const char* input_ext = file_path_ext(input);
    if (!paged && input_ext && !strcmp(input_ext, ".pdf") && !strcmp(ext, ".pdf")) {
        FileCopyOptions copy_options = {true, false};
        return file_copy(input, output, &copy_options) == 0 ? 0 : 1;
    }
    const char* transform_type = graph_path_is_graph(input) ? "graph" :
        eps_source_expected(input, nullptr, 0) ? "eps" :
        input_ext && !strcmp(input_ext, ".pgf") ? "tikz" : nullptr;
    if (transform_type) {
        const LambdaDocumentTransformConfig* transform = radiant_loader_transform_for_type(transform_type);
        if (!transform) { fprintf(stderr, "Document transform unavailable: %s\n", transform_type); return 1; }
        LambdaDocumentTransformOption options[2] = {};
        int count = 0;
        if (theme) options[count++] = {"theme", LAMBDA_DOCUMENT_TRANSFORM_OPTION_STRING, theme, false};
        if (view_key) options[count++] = {"view_key", LAMBDA_DOCUMENT_TRANSFORM_OPTION_STRING, view_key, false};
        return render_document_transform_to_output_target(input, transform, options, count,
            output, width, height, scale, device_scale, 85, paged, &paged_options);
    }
    return render_html_to_output_target(input, output, width, height, scale, device_scale, 85, paged, &paged_options);
}

int main(int argc, char** argv) {
    if (argc < 2) { usage(stderr); return 1; }
    if (!strcmp(argv[1], "--help") || !strcmp(argv[1], "-h")) { usage(stdout); return 0; }
    if (!strcmp(argv[1], "layout") && argc >= 3 &&
        (!strcmp(argv[2], "--help") || !strcmp(argv[2], "-h"))) {
        fputs("Usage: radiant.exe layout <document> [more documents] [options]\n"
              "  --view-output FILE       Write a single layout tree\n"
              "  --output-dir DIR         Write batch layout trees\n"
              "  -c, --css FILE           Apply a stylesheet\n"
              "  -vw, --viewport-width N  Viewport width\n"
              "  -vh, --viewport-height N Viewport height\n"
              "  --auto-close             Complete headless script processing\n"
              "  --post-load-settle-ms N  Run delayed document tasks\n"
              "  --no-log                 Disable logging\n", stdout);
        return 0;
    }
    bool no_log = false;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--no-log")) continue;
        no_log = true;
        for (int j = i; j < argc; j++) argv[j] = argv[j + 1];
        argc--; i--;
    }
    lambda_home_init();
    log_set_output_dir(host_path(HOST_PATH_LOG));
    log_init("");
    if (no_log) log_disable_all();
    if (!radiant_register_dom_engine_providers()) return 1;
    const char* interp = getenv("LAMBDA_JS_LARGE_INTERP");
    if (!interp || !*interp || (strcmp(interp, "0") && strcmp(interp, "false")))
        g_js_force_document_interp = 1;
    int result;
    if (!strcmp(argv[1], "layout")) result = cmd_layout(argc - 2, argv + 2);
    else if (!strcmp(argv[1], "render")) result = command_render(argc - 2, argv + 2);
    else if (!strcmp(argv[1], "render-batch")) result = cmd_render_batch(argc - 2, argv + 2);
    else { usage(stderr); result = 1; }
    if (!radiant_window_host_process_shutdown_ready()) return result ? result : 1;
    if (lambda_uv_loop() && !lambda_uv_is_host_owned()) lambda_uv_cleanup();
    clipboard_store_shutdown();
    css_property_system_cleanup();
    radiant_state_cleanup_interned_names();
    module_ast_prebuild_cleanup();
    InputManager::destroy_global();
    mem_context_shutdown();
    log_finish();
    return result;
}
