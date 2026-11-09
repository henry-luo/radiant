#pragma once

#include "lambda/io/resource_policy.h"

struct Item;
struct UiContext;
struct DomDocument;
struct Runtime;
struct EvalContext;
struct LambdaDocumentLifetimeGuard;
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

// Runtime owners are opaque to the native document lifecycle.
struct RadiantDocumentCleanupOwners {
    EvalContext* timer;
    EvalContext* state;
    EvalContext* loader;
};

enum RadiantDocumentJoinProgress {
    RADIANT_DOCUMENT_JOIN_ERROR,
    RADIANT_DOCUMENT_JOIN_DONE,
    RADIANT_DOCUMENT_JOIN_WAITING,
    RADIANT_DOCUMENT_JOIN_ADVANCED,
};

enum RadiantRuntimeCleanupResult {
    RADIANT_RUNTIME_CLEANUP_COMPLETE,
    RADIANT_RUNTIME_CLEANUP_RETRY,
    RADIANT_RUNTIME_CLEANUP_INCOMPLETE,
};

typedef Item (*RadiantEventEmitFn)(Item event_name, Item event_data);

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
    Runtime* (*defer_html_script_cleanup)(DomDocument* document);
    bool (*cleanup_owners)(DomDocument* document, RadiantDocumentCleanupOwners* owners);
    void (*release_observers)(DomDocument* document);
    void (*release_dom)(DomDocument* document);
    void (*cancel_timers)(DomDocument* document, EvalContext* owner);
    bool (*prepare_cleanup)(EvalContext* timer, EvalContext* state);
    bool (*bind_owner)(EvalContext* owner);
    bool (*switch_context)(EvalContext* owner);
    bool (*release_navigation_evaluator)();
    bool (*finish_navigation_worker)();
    bool (*bind_document)(UiContext* ui, DomDocument* document);
    bool (*bind_event_document)(UiContext* ui, DomDocument* document, bool allow_transfer, bool rebind_same);
    bool (*register_event_hooks)(RadiantEventEmitFn emit);
    EvalContext* (*runtime_owner)(Runtime* runtime);
    bool (*runtime_is_cleaning)(Runtime* runtime);
    void (*detach_document)(Runtime* runtime, DomDocument* document);
    void (*cleanup_runtime)(Runtime* runtime);
    RadiantRuntimeCleanupResult (*try_cleanup_runtime)(Runtime* runtime, bool wait, bool pinned);
    int (*release_document_tasks)(DomDocument* document);
    bool (*pending_native_work)(EvalContext* owner, DomDocument* document);
    bool (*pin_runtime)(Runtime* runtime);
    bool (*unpin_runtime)(Runtime* runtime);
    bool (*cancel_document_tasks)(EvalContext* owner, DomDocument* document);
    RadiantDocumentJoinProgress (*progress_document_tasks)(EvalContext* owner, DomDocument* document,
                                                            LambdaDocumentLifetimeGuard* guard);
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

bool radiant_loader_cleanup_owners(DomDocument* document, RadiantDocumentCleanupOwners* owners);
void radiant_loader_release_observers(DomDocument* document);
void radiant_loader_release_dom(DomDocument* document);
void radiant_loader_cancel_document_timers(DomDocument* document, EvalContext* owner);
bool radiant_loader_prepare_cleanup(EvalContext* timer, EvalContext* state);
bool radiant_loader_bind_owner(EvalContext* owner);
bool radiant_loader_switch_context(EvalContext* owner);
EvalContext* radiant_loader_runtime_owner(Runtime* runtime);
bool radiant_loader_runtime_is_cleaning(Runtime* runtime);
void radiant_loader_detach_document(Runtime* runtime, DomDocument* document);
bool radiant_loader_cleanup_runtime(Runtime* runtime);
RadiantRuntimeCleanupResult radiant_loader_try_cleanup_runtime(Runtime* runtime, bool wait, bool pinned);
int radiant_loader_release_document_tasks(DomDocument* document);
bool radiant_loader_pending_native_work(EvalContext* owner, DomDocument* document);
bool radiant_loader_pin_runtime(Runtime* runtime);
bool radiant_loader_unpin_runtime(Runtime* runtime);
bool radiant_loader_cancel_document_tasks(EvalContext* owner, DomDocument* document);
RadiantDocumentJoinProgress radiant_loader_progress_document_tasks(EvalContext* owner, DomDocument* document,
                                                                   LambdaDocumentLifetimeGuard* guard);
Runtime* radiant_loader_defer_html_script_cleanup(DomDocument* document);
bool radiant_loader_release_navigation_evaluator(DomDocument* document);
bool radiant_loader_finish_navigation_worker(DomDocument* document);
bool radiant_loader_bind_document(UiContext* ui, DomDocument* document);
bool radiant_loader_bind_event_document(UiContext* ui, DomDocument* document, bool allow_transfer, bool rebind_same);
bool radiant_loader_register_event_hooks(RadiantEventEmitFn emit);
