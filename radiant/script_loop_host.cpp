#include "script_loop_host.hpp"
#include "lib/provider.hpp"
#include "lambda/input/css/dom_element.hpp"

static const RadiantScriptLoopHostOps* script_loop_host_ops = nullptr;

bool radiant_script_loop_host_register(const RadiantScriptLoopHostOps* operations) {
    return lambda_provider_register(&script_loop_host_ops, operations,
        &RadiantScriptLoopHostOps::begin, &RadiantScriptLoopHostOps::pump,
        &RadiantScriptLoopHostOps::end, &RadiantScriptLoopHostOps::bind_document,
        &RadiantScriptLoopHostOps::cancel_document, &RadiantScriptLoopHostOps::quiescent,
        &RadiantScriptLoopHostOps::detach_window, &RadiantScriptLoopHostOps::cleanup_batch,
        &RadiantScriptLoopHostOps::reset_clock, &RadiantScriptLoopHostOps::animation_frame,
        &RadiantScriptLoopHostOps::configure, &RadiantScriptLoopHostOps::progress_generation,
        &RadiantScriptLoopHostOps::edit_transform);
}

bool radiant_script_loop_begin(UiContext* ui, DomDocument* doc, RadiantScriptLoopTurn* turn) {
    if (!turn) return false;
    *turn = {};
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return operations ? operations->begin(ui, doc, turn) : !dom_document_script_runtime(doc);
}

bool radiant_script_loop_pump(const RadiantScriptLoopTurn* turn, bool advance,
                              int wait_ms, double delta_ms, int frame_steps) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return operations && operations->pump(turn && turn->active, advance, wait_ms, delta_ms, frame_steps);
}

void radiant_script_loop_end(RadiantScriptLoopTurn* turn) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    if (operations && turn) operations->end(turn);
}

bool radiant_script_window_bind_document(DomDocument* doc) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return operations ? operations->bind_document(doc) : !dom_document_script_runtime(doc);
}

void radiant_script_window_cancel_document(DomDocument* doc) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    if (operations) operations->cancel_document(doc);
}

bool radiant_script_window_quiescent() {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return !operations || operations->quiescent();
}

void radiant_script_window_detach() {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    if (operations) operations->detach_window();
}

void radiant_script_window_cleanup_batch(bool reset_dom_cache) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    if (operations) operations->cleanup_batch(reset_dom_cache);
}

void radiant_script_window_reset_clock() {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    if (operations) operations->reset_clock();
}

bool radiant_script_window_animation_frame(double time_ms, bool* callbacks_ran) {
    if (callbacks_ran) *callbacks_ran = false;
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return operations && operations->animation_frame(time_ms, callbacks_ran);
}

void radiant_script_window_configure(bool retain, bool static_snapshot, bool external_scripts) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    if (operations) operations->configure(retain, static_snapshot, external_scripts);
}

uint64_t radiant_script_window_progress_generation() {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return operations ? operations->progress_generation() : 0;
}

bool radiant_script_window_edit_transform(bool source, const LambdaDocumentTransformConfig** transform,
                                          const LambdaDocumentTransformOption** options, int* option_count) {
    const RadiantScriptLoopHostOps* operations = lambda_provider_load(&script_loop_host_ops);
    return operations && operations->edit_transform(source, transform, options, option_count);
}
