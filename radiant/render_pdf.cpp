// PDF export uses an optional script host for its writer (S2.6.6v3).
#include "render.hpp"
#include "layout.hpp"
#include "event.hpp"
#include "view_tree_model.hpp"
#include "pdf_export_host.hpp"
#include "../lib/log.h"

bool render_secondary_view_to_pdf(ViewTree* tree, const char* filename,
        const ViewPageSelection* selection, UiContext* ui) {
    if (!tree || !tree->model || !tree->model->committed || !view_tree_model_source_valid(tree) ||
        tree->model->environment.presentation != VIEW_PRESENTATION_PAGED) return false;
    log_info("PDF_LAMBDA_SOURCE: physical pages=%zu", tree->model->page_count);
    RenderExportSession session = {};
    session.document = lam::up(tree->model->document.get());
    // embedding callers may omit the host already retained by the edition environment.
    session.ui_context = lam::up(ui ? ui : tree->model->environment.ui_context.get());
    session.paged_view = lam::up(tree); session.output_scale = 1;
    return radiant_pdf_export_write(&session, filename, selection);
}

int render_export_session_to_pdf(RenderExportSession* session, const char* filename) {
    return radiant_pdf_export_write(session, filename, nullptr) ? 0 : 1;
}

int render_uicontext_to_pdf(UiContext* ui, const char* filename) {
    if (!ui || !ui->document || !ui->document->view_tree) return 1;
    // borrow the settled live edition; capture must not reload scripts or clocks.
    RenderExportSession session = {};
    session.ui_context = lam::up(ui);
    session.document = lam::up(ui->document.get());
    session.output_scale = ui_context_raster_scale(ui);
    session.content_width = ui->viewport_width;
    session.content_height = ui->viewport_height;
    calculate_content_bounds(ui->document->view_tree->root,
        &session.content_width, &session.content_height);
    session.content_width += 50;
    session.content_height += 50;
    return render_export_session_to_pdf(&session, filename);
}

int render_html_to_pdf(const char* source, const char* filename, int width,
        int height, float scale, bool paged, float device_scale) {
    RenderExportSession session = {};
    if (!render_export_session_begin(&session, source, width, height, 800, 1200, scale, true, paged, device_scale)) return 1;
    int status = render_export_session_to_pdf(&session, filename);
    render_export_session_finish(&session); return status;
}

int render_document_transform_to_pdf(const char* source, const LambdaDocumentTransformConfig* transform,
        const LambdaDocumentTransformOption* options, int count, const char* filename,
        int width, int height, float scale, bool paged, float device_scale) {
    RenderExportSession session = {};
    if (!render_export_session_begin_document_transform(&session, source, transform, options, count,
            width, height, 800, 1200, scale, device_scale, false, paged, true)) return 1;
    int status = render_export_session_to_pdf(&session, filename);
    render_export_session_finish(&session); return status;
}
