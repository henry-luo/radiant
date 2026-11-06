#pragma once

struct RenderExportSession;
struct ViewPageSelection;

// Script-backed PDF writers are optional hosts of the native export session.
struct RadiantPdfExportHostOps {
    bool (*write)(RenderExportSession* session, const char* filename, const ViewPageSelection* selection);
};

bool radiant_pdf_export_host_register(const RadiantPdfExportHostOps* operations);
bool radiant_pdf_export_write(RenderExportSession* session, const char* filename,
                              const ViewPageSelection* selection);
