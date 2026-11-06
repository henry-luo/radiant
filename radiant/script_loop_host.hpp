#pragma once

#include <stdint.h>

struct UiContext;
struct DomDocument;
struct LambdaDocumentTransformConfig;
struct LambdaDocumentTransformOption;

// A turn retains the script host's previous input binding until completion.
struct RadiantScriptLoopTurn {
    void* saved_input;
    bool active;
};

struct RadiantScriptLoopHostOps {
    bool (*begin)(UiContext*, DomDocument*, RadiantScriptLoopTurn*);
    bool (*pump)(bool active, bool advance, int wait_ms, double delta_ms, int frame_steps);
    void (*end)(RadiantScriptLoopTurn*);
    bool (*bind_document)(DomDocument*);
    void (*cancel_document)(DomDocument*);
    bool (*quiescent)();
    void (*detach_window)();
    void (*cleanup_batch)(bool reset_dom_cache);
    void (*reset_clock)();
    bool (*animation_frame)(double time_ms, bool* callbacks_ran);
    void (*configure)(bool retain, bool static_snapshot, bool external_scripts);
    uint64_t (*progress_generation)();
    bool (*edit_transform)(bool source, const LambdaDocumentTransformConfig** transform,
                           const LambdaDocumentTransformOption** options, int* option_count);
};

bool radiant_script_loop_host_register(const RadiantScriptLoopHostOps* operations);
bool radiant_script_loop_begin(UiContext* ui, DomDocument* doc, RadiantScriptLoopTurn* turn);
bool radiant_script_loop_pump(const RadiantScriptLoopTurn* turn, bool advance,
                              int wait_ms, double delta_ms, int frame_steps);
void radiant_script_loop_end(RadiantScriptLoopTurn* turn);
bool radiant_script_window_bind_document(DomDocument* doc);
void radiant_script_window_cancel_document(DomDocument* doc);
bool radiant_script_window_quiescent();
void radiant_script_window_detach();
void radiant_script_window_cleanup_batch(bool reset_dom_cache);
void radiant_script_window_reset_clock();
bool radiant_script_window_animation_frame(double time_ms, bool* callbacks_ran);
void radiant_script_window_configure(bool retain, bool static_snapshot, bool external_scripts);
uint64_t radiant_script_window_progress_generation();
bool radiant_script_window_edit_transform(bool source, const LambdaDocumentTransformConfig** transform,
                                          const LambdaDocumentTransformOption** options, int* option_count);
