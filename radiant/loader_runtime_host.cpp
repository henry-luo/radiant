#include "loader_runtime_host.hpp"
#include "../lib/provider.hpp"
#include "../lib/log.h"
#include "../lambda/input/css/dom_element.hpp"

static const RadiantLoaderRuntimeHostOps* loader_runtime_host_ops = nullptr;

bool radiant_loader_runtime_host_register(const RadiantLoaderRuntimeHostOps* operations) {
    return lambda_provider_register(&loader_runtime_host_ops, operations,
        &RadiantLoaderRuntimeHostOps::get, &RadiantLoaderRuntimeHostOps::finish_document,
        &RadiantLoaderRuntimeHostOps::release, &RadiantLoaderRuntimeHostOps::begin_script,
        &RadiantLoaderRuntimeHostOps::cancel_script, &RadiantLoaderRuntimeHostOps::finish_script,
        &RadiantLoaderRuntimeHostOps::prepare_script, &RadiantLoaderRuntimeHostOps::render_embedded,
        &RadiantLoaderRuntimeHostOps::transform_for_type,
        &RadiantLoaderRuntimeHostOps::execute_html_scripts, &RadiantLoaderRuntimeHostOps::compile_html_handlers,
        &RadiantLoaderRuntimeHostOps::begin_document_timing, &RadiantLoaderRuntimeHostOps::end_document_timing,
        &RadiantLoaderRuntimeHostOps::cleanup_html_script, &RadiantLoaderRuntimeHostOps::defer_html_script_cleanup,
        &RadiantLoaderRuntimeHostOps::cleanup_owners,
        &RadiantLoaderRuntimeHostOps::release_observers, &RadiantLoaderRuntimeHostOps::release_dom,
        &RadiantLoaderRuntimeHostOps::cancel_timers, &RadiantLoaderRuntimeHostOps::prepare_cleanup,
        &RadiantLoaderRuntimeHostOps::bind_owner, &RadiantLoaderRuntimeHostOps::switch_context,
        &RadiantLoaderRuntimeHostOps::release_navigation_evaluator, &RadiantLoaderRuntimeHostOps::finish_navigation_worker,
        &RadiantLoaderRuntimeHostOps::bind_document, &RadiantLoaderRuntimeHostOps::bind_event_document,
        &RadiantLoaderRuntimeHostOps::register_event_hooks,
        &RadiantLoaderRuntimeHostOps::runtime_owner, &RadiantLoaderRuntimeHostOps::runtime_is_cleaning,
        &RadiantLoaderRuntimeHostOps::detach_document, &RadiantLoaderRuntimeHostOps::cleanup_runtime,
        &RadiantLoaderRuntimeHostOps::try_cleanup_runtime, &RadiantLoaderRuntimeHostOps::release_document_tasks,
        &RadiantLoaderRuntimeHostOps::pending_native_work, &RadiantLoaderRuntimeHostOps::pin_runtime,
        &RadiantLoaderRuntimeHostOps::unpin_runtime, &RadiantLoaderRuntimeHostOps::cancel_document_tasks,
        &RadiantLoaderRuntimeHostOps::progress_document_tasks);
}

bool radiant_loader_runtime_host_available() {
    return lambda_provider_load(&loader_runtime_host_ops) != nullptr;
}

Runtime* ui_context_loader_runtime(UiContext* host) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->get(host) : nullptr;
}

void loader_runtime_finish_document(DomDocument* document, Runtime* runtime) {
    if (!runtime) return;
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) operations->finish_document(document, runtime);
    else log_error("loader-runtime: cannot release document roots without their host");
}

void radiant_loader_runtime_release(Runtime* runtime) {
    if (!runtime) return;
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) operations->release(runtime);
    else log_error("loader-runtime: cannot release evaluator without its host");
}

bool radiant_loader_begin_script(UiContext* host, Url* url, const char* path,
                                 const LambdaDocumentTransformConfig* transform,
                                 const LambdaDocumentTransformOption* options, int option_count,
                                 InputResourcePolicy policy, RadiantScriptDocumentLoad* load) {
    if (!load) return false;
    *load = {};
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (!operations) {
        log_error("document-load: no scripting host is available");
        return false;
    }
    return operations->begin_script(host, url, path, transform, options, option_count, policy, load);
}

void radiant_loader_cancel_script(RadiantScriptDocumentLoad* load, bool runtime_released) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations && load) operations->cancel_script(load, runtime_released);
}

void radiant_loader_finish_script(RadiantScriptDocumentLoad* load, DomDocument* document, Element* root) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations && load) operations->finish_script(load, document, root);
}

bool radiant_loader_prepare_script() {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) return operations->prepare_script();
    log_error("document-load: no scripting host is available");
    return false;
}

bool radiant_loader_render_embedded(UiContext* host, Input* input, Runtime** owned, Runtime** shared) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) return operations->render_embedded(host, input, owned, shared);
    log_error("document-load: embedded math or diagrams need a scripting host");
    return false;
}

const LambdaDocumentTransformConfig* radiant_loader_transform_for_type(const char* input_type) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->transform_for_type(input_type) : nullptr;
}

void radiant_loader_execute_html_scripts(Element* root, DomDocument* document, Pool* pool,
                                         Url* base_url, DocumentScriptPhaseTiming* timing) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) operations->execute_html_scripts(root, document, pool, base_url, timing);
    else log_info("document-load: HTML scripting is unavailable without a host");
}

void radiant_loader_compile_html_handlers(DomDocument* document) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) operations->compile_html_handlers(document);
}

void radiant_loader_begin_document_timing() {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) operations->begin_document_timing();
}

void radiant_loader_end_document_timing(RadiantScriptPhaseSummary* timing) {
    if (!timing) return;
    *timing = {};
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) operations->end_document_timing(timing);
}

bool radiant_loader_cleanup_html_script(DomDocument* document) {
    if (!document) return true;
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) {
        operations->cleanup_html_script(document);
        return true;
    }
    // Never retire a document while a missing host still owns its script state.
    if (document->js.runtime || document->js.preamble_state) {
        log_error("document-load: cannot retire script state without its host");
        return false;
    }
    return true;
}

bool radiant_loader_cleanup_owners(DomDocument* document, RadiantDocumentCleanupOwners* owners) {
    if (!owners) return false;
    *owners = {};
    if (!document) return true;
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) return operations->cleanup_owners(document, owners);
    // Runtime-owned documents cannot be safely retired without their host.
    if (document->js.runtime || document->js.preamble_state ||
            document->lambda_runtime || document->loader_runtime) {
        log_error("document-cleanup: missing runtime host; document retained");
        return false;
    }
    return true;
}

void radiant_loader_release_observers(DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::release_observers, document);
}

void radiant_loader_release_dom(DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::release_dom, document);
}

void radiant_loader_cancel_document_timers(DomDocument* document, EvalContext* owner) {
    lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::cancel_timers, document, owner);
}

bool radiant_loader_prepare_cleanup(EvalContext* timer, EvalContext* state) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->prepare_cleanup(timer, state) : !timer && !state;
}

bool radiant_loader_bind_owner(EvalContext* owner) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::bind_owner, owner);
}

bool radiant_loader_switch_context(EvalContext* owner) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::switch_context, owner);
}

EvalContext* radiant_loader_runtime_owner(Runtime* runtime) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations && runtime ? operations->runtime_owner(runtime) : nullptr;
}

bool radiant_loader_runtime_is_cleaning(Runtime* runtime) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return !operations || operations->runtime_is_cleaning(runtime);
}

void radiant_loader_detach_document(Runtime* runtime, DomDocument* document) {
    lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::detach_document, runtime, document);
}

bool radiant_loader_cleanup_runtime(Runtime* runtime) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (!operations) return false;
    operations->cleanup_runtime(runtime);
    return true;
}

RadiantRuntimeCleanupResult radiant_loader_try_cleanup_runtime(Runtime* runtime, bool wait, bool pinned) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->try_cleanup_runtime(runtime, wait, pinned) : RADIANT_RUNTIME_CLEANUP_INCOMPLETE;
}

int radiant_loader_release_document_tasks(DomDocument* document) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->release_document_tasks(document) : -1;
}

bool radiant_loader_pending_native_work(EvalContext* owner, DomDocument* document) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->pending_native_work(owner, document) : owner != nullptr;
}

bool radiant_loader_pin_runtime(Runtime* runtime) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::pin_runtime, runtime);
}

bool radiant_loader_unpin_runtime(Runtime* runtime) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::unpin_runtime, runtime);
}

bool radiant_loader_cancel_document_tasks(EvalContext* owner, DomDocument* document) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::cancel_document_tasks, owner, document);
}

RadiantDocumentJoinProgress radiant_loader_progress_document_tasks(EvalContext* owner, DomDocument* document,
                                                                   LambdaDocumentLifetimeGuard* guard) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations ? operations->progress_document_tasks(owner, document, guard) : RADIANT_DOCUMENT_JOIN_ERROR;
}

Runtime* radiant_loader_defer_html_script_cleanup(DomDocument* document) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::defer_html_script_cleanup, document);
}

bool radiant_loader_release_navigation_evaluator(DomDocument* document) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) return operations->release_navigation_evaluator();
    // A native-only document has no evaluator; script ownership requires a host.
    return document && !document->js.runtime && !document->js.preamble_state &&
        !document->lambda_runtime && !document->loader_runtime;
}

bool radiant_loader_finish_navigation_worker(DomDocument* document) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    if (operations) return operations->finish_navigation_worker();
    return radiant_loader_release_navigation_evaluator(document);
}

bool radiant_loader_bind_document(UiContext* ui, DomDocument* document) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::bind_document, ui, document);
}

bool radiant_loader_bind_event_document(UiContext* ui, DomDocument* document, bool allow_transfer, bool rebind_same) {
    return lambda_provider_call(lambda_provider_load(&loader_runtime_host_ops),
        &RadiantLoaderRuntimeHostOps::bind_event_document, ui, document, allow_transfer, rebind_same);
}

bool radiant_loader_register_event_hooks(RadiantEventEmitFn emit) {
    const RadiantLoaderRuntimeHostOps* operations = lambda_provider_load(&loader_runtime_host_ops);
    return operations && emit && operations->register_event_hooks(emit);
}
