#include "package_layout_host.hpp"
#include "lib/provider.hpp"

static const RadiantPackageLayoutHostOps* package_layout_host_ops = nullptr;

bool radiant_package_layout_host_register(const RadiantPackageLayoutHostOps* operations) {
    return lambda_provider_register(&package_layout_host_ops, operations, &RadiantPackageLayoutHostOps::invoke);
}

bool radiant_package_layout_callback(const CustomLayoutContext* context,
                                     CustomLayoutResult* result, const char* module) {
    const RadiantPackageLayoutHostOps* operations = lambda_provider_load(&package_layout_host_ops);
    return operations && operations->invoke(context, result, module);
}
