#pragma once

#include "../lambda/io/resource_policy.h"

struct UiContext;
struct DomDocument;
struct Runtime;
struct Url;
struct Pool;
struct Input;
struct Element;
struct LambdaDocumentTransformConfig;
struct LambdaDocumentTransformOption;
struct DocumentScriptPhaseTiming;

struct RadiantScriptDocumentLoad {
    Runtime* runtime;
    Pool* result_pool;
    Input* result_input;
    Input* script_output;
    bool shared;
    bool stateless;
};

// Native timing data carries no script runtime objects across the host boundary.
struct RadiantScriptPhaseSummary {
    long parse_us, ast_us, early_us, imports_us, mir_us, link_us;
    long execute_us, cleanup_us, total_us, preamble_us;
};

// The host owns evaluators and any precise roots published by a loader.
struct RadiantLoaderRuntimeHostOps {
    Runtime* (*get)(UiContext* host);
    void (*finish_document)(DomDocument* document, Runtime* runtime);
    void (*release)(Runtime* runtime);
    bool (*begin_script)(UiContext* host, Url* url, const char* path,
                         const LambdaDocumentTransformConfig* transform,
                         const LambdaDocumentTransformOption* options, int option_count,
                         InputResourcePolicy policy, RadiantScriptDocumentLoad* load);
    void (*cancel_script)(RadiantScriptDocumentLoad* load, bool runtime_released);
    void (*finish_script)(RadiantScriptDocumentLoad* load, DomDocument* document, Element* root);
    bool (*prepare_script)();
    bool (*render_embedded)(UiContext* host, Input* input, Runtime** owned, Runtime** shared);
    const LambdaDocumentTransformConfig* (*transform_for_type)(const char* input_type);
    void (*execute_html_scripts)(Element* root, DomDocument* document, Pool* pool,
                                 Url* base_url, DocumentScriptPhaseTiming* timing);
    void (*compile_html_handlers)(DomDocument* document);
    void (*begin_document_timing)();
    void (*end_document_timing)(RadiantScriptPhaseSummary* timing);
    void (*cleanup_html_script)(DomDocument* document);
};

bool radiant_loader_runtime_host_register(const RadiantLoaderRuntimeHostOps* operations);
bool radiant_loader_runtime_host_available();
Runtime* ui_context_loader_runtime(UiContext* host);
void loader_runtime_finish_document(DomDocument* document, Runtime* runtime);
void radiant_loader_runtime_release(Runtime* runtime);
bool radiant_loader_begin_script(UiContext* host, Url* url, const char* path,
                                 const LambdaDocumentTransformConfig* transform,
                                 const LambdaDocumentTransformOption* options, int option_count,
                                 InputResourcePolicy policy, RadiantScriptDocumentLoad* load);
void radiant_loader_cancel_script(RadiantScriptDocumentLoad* load, bool runtime_released);
void radiant_loader_finish_script(RadiantScriptDocumentLoad* load, DomDocument* document, Element* root);
bool radiant_loader_prepare_script();
bool radiant_loader_render_embedded(UiContext* host, Input* input, Runtime** owned, Runtime** shared);
const LambdaDocumentTransformConfig* radiant_loader_transform_for_type(const char* input_type);
void radiant_loader_execute_html_scripts(Element* root, DomDocument* document, Pool* pool,
                                         Url* base_url, DocumentScriptPhaseTiming* timing);
void radiant_loader_compile_html_handlers(DomDocument* document);
void radiant_loader_begin_document_timing();
void radiant_loader_end_document_timing(RadiantScriptPhaseSummary* timing);
bool radiant_loader_cleanup_html_script(DomDocument* document);
