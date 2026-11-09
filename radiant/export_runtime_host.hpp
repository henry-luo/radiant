#pragma once

#include "lambda/io/resource_policy.h"

struct DomDocument;
struct RenderExportSession;
struct LambdaDocumentTransformConfig;
struct LambdaDocumentTransformOption;

// A host may retain its evaluator across an export and deferred retirement.
struct RadiantExportRuntimeHostOps {
    bool (*acquire)(void** owner);
    bool (*release)(void* owner);
    bool (*retirement_blocked)();
    DomDocument* (*load_transform)(RenderExportSession* session, int width, int height,
        const char* document_file, const LambdaDocumentTransformConfig* transform,
        const LambdaDocumentTransformOption* options, int option_count, InputResourcePolicy policy);
};

bool radiant_export_runtime_host_register(const RadiantExportRuntimeHostOps* operations);
bool radiant_export_runtime_acquire(void** owner);
bool radiant_export_runtime_release(void* owner);
bool radiant_export_runtime_retirement_blocked();

DomDocument* radiant_export_runtime_load_transform(RenderExportSession* session, int width, int height,
    const char* document_file, const LambdaDocumentTransformConfig* transform,
    const LambdaDocumentTransformOption* options, int option_count, InputResourcePolicy policy);
