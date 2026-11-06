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
        &RadiantLoaderRuntimeHostOps::cleanup_html_script);
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
