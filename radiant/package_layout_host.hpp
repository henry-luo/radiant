#pragma once

struct CustomLayoutContext;
struct CustomLayoutResult;

// The host may supply Lambda package layouts; native layouts need no script runtime.
struct RadiantPackageLayoutHostOps {
    bool (*invoke)(const CustomLayoutContext* context, CustomLayoutResult* result, const char* module);
};

bool radiant_package_layout_host_register(const RadiantPackageLayoutHostOps* operations);
bool radiant_package_layout_callback(const CustomLayoutContext* context,
                                     CustomLayoutResult* result, const char* module);
