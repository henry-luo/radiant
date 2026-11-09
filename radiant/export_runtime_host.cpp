#include "export_runtime_host.hpp"
#include "lib/provider.hpp"

static const RadiantExportRuntimeHostOps* export_runtime_host_ops = nullptr;

bool radiant_export_runtime_host_register(const RadiantExportRuntimeHostOps* operations) {
    return lambda_provider_register(&export_runtime_host_ops, operations,
        &RadiantExportRuntimeHostOps::acquire, &RadiantExportRuntimeHostOps::release,
        &RadiantExportRuntimeHostOps::retirement_blocked, &RadiantExportRuntimeHostOps::load_transform);
}

bool radiant_export_runtime_acquire(void** owner) {
    if (!owner) return false;
    *owner = nullptr;
    const RadiantExportRuntimeHostOps* operations = lambda_provider_load(&export_runtime_host_ops);
    return !operations || operations->acquire(owner);
}

bool radiant_export_runtime_release(void* owner) {
    if (!owner) return true;
    const RadiantExportRuntimeHostOps* operations = lambda_provider_load(&export_runtime_host_ops);
    return operations && operations->release(owner);
}

bool radiant_export_runtime_retirement_blocked() {
    const RadiantExportRuntimeHostOps* operations = lambda_provider_load(&export_runtime_host_ops);
    return operations && operations->retirement_blocked();
}

DomDocument* radiant_export_runtime_load_transform(RenderExportSession* session, int width, int height,
    const char* document_file, const LambdaDocumentTransformConfig* transform,
    const LambdaDocumentTransformOption* options, int option_count, InputResourcePolicy policy) {
    const RadiantExportRuntimeHostOps* operations = lambda_provider_load(&export_runtime_host_ops);
    return operations ? operations->load_transform(session, width, height, document_file,
        transform, options, option_count, policy) : nullptr;
}
