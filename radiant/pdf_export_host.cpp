#include "pdf_export_host.hpp"
#include "lib/provider.hpp"

static const RadiantPdfExportHostOps* pdf_export_host_ops = nullptr;

bool radiant_pdf_export_host_register(const RadiantPdfExportHostOps* operations) {
    return lambda_provider_register(&pdf_export_host_ops, operations, &RadiantPdfExportHostOps::write);
}

bool radiant_pdf_export_write(RenderExportSession* session, const char* filename,
                              const ViewPageSelection* selection) {
    const RadiantPdfExportHostOps* operations = lambda_provider_load(&pdf_export_host_ops);
    return operations && operations->write(session, filename, selection);
}
