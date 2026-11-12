// composition-only samples; loading, hashing and edition retirement stay outside the timer.
#include "radiant/render.hpp"
#include "radiant/layout_paged.hpp"
#include "radiant/view_tree_model.hpp"
#include "lib/hash.h"
#include "lib/time_util.h"
#include "lib/strbuf.h"
#include <stdio.h>
#include <stdlib.h>

static uint64_t edition_hash(ViewTree* tree) {
    uint64_t hash = HASH_FNV1A_64_OFFSET_BASIS;
    for (size_t i = 0; i < tree->model->node_id_count; i++) {
        const LayoutViewNode* node = tree->model->nodes.get()[i];
        if (!node) continue;
        hash = hash_fnv1a_64_extend_u64le(hash, node->kind);
        hash = hash_fnv1a_64_extend_u64le(hash, node->role);
        hash = hash_fnv1a_64_extend(hash, &node->rect, sizeof(node->rect));
        hash = hash_fnv1a_64_extend_u64le(hash, node->text_start);
        hash = hash_fnv1a_64_extend_u64le(hash, node->text_length);
        const PaintGlyphRun* run = node->glyph_run;
        if (!run) continue;
        size_t bytes = run->text_len < 0 ? strlen(run->text) : (size_t)run->text_len;
        if (run->text) hash = hash_fnv1a_64_extend(hash, run->text.get(), bytes);
        hash = hash_fnv1a_64_extend(hash, &run->x, sizeof(run->x));
        hash = hash_fnv1a_64_extend(hash, &run->baseline_y, sizeof(run->baseline_y));
        for (int glyph = 0; glyph < run->count; glyph++) {
            if (run->glyph_ids) hash = hash_fnv1a_64_extend_u64le(hash, run->glyph_ids[glyph]);
            if (run->xs) hash = hash_fnv1a_64_extend(hash, &run->xs[glyph], sizeof(float));
            if (run->ys) hash = hash_fnv1a_64_extend(hash, &run->ys[glyph], sizeof(float));
        }
    }
    return hash;
}

int main(int argc, char** argv) {
#ifndef NDEBUG
    fputs("composition benchmark requires PROFILE=release or release_profile\n", stderr);
    return 2;
#else
    if (argc != 3) return 2;
    char* end = nullptr;
    unsigned long samples = strtoul(argv[2], &end, 10);
    if (!end || *end || samples < 1 || samples > 1000) return 2;
    RenderExportSession session = {};
    if (!render_export_session_begin(&session, argv[1], 0, 0, 640, 800, 1.0f, true, true, 1.0f, false)) return 1;
    ViewEnvironment environment = session.paged_view->model->environment;
    view_tree_secondary_release(session.document, session.paged_view);
    session.paged_view = nullptr;
    PagedLayoutOptions options = paged_layout_options_default();
    StrBuf* result = strbuf_new();
    if (!result) { render_export_session_finish(&session); return 1; }
    strbuf_append_str(result, "{\"profile\":\"optimized-NDEBUG\",\"warmups\":2,\"samples\":[");
    bool ok = true;
    for (unsigned long sample = 0; sample < samples + 2 && ok; sample++) {
        ViewTree* tree = view_tree_secondary_create(session.document, &environment);
        if (!tree) { ok = false; break; }
        PagedLayoutDiagnostic diagnostic = {};
        uint64_t start = time_now_ns();
        TypesetStatus status = layout_secondary_view(tree, &options, &diagnostic);
        uint64_t elapsed = time_elapsed_ns(start, time_now_ns());
        if (status != TYPESET_OK) ok = false;
        else if (sample >= 2) {
            strbuf_append_format(result, "%s{\"ns\":%llu,\"pages\":%zu,\"reference_passes\":%u,\"edition_fnv1a64\":\"%016llx\"}",
                sample > 2 ? "," : "", (unsigned long long)elapsed, tree->model->page_count,
                diagnostic.reference_passes, (unsigned long long)edition_hash(tree));
        }
        view_tree_secondary_release(session.document, tree);
    }
    strbuf_append_str(result, "]}\n");
    if (ok) fwrite(result->str, 1, result->length, stdout);
    strbuf_free(result);
    render_export_session_finish(&session);
    return ok ? 0 : 1;
#endif
}
