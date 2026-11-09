#include "devtools_adapter.hpp"
#include "lib/provider.hpp"

static const RadiantDevtoolsHostOps* devtools_host_ops = nullptr;

bool radiant_devtools_host_register(const RadiantDevtoolsHostOps* operations) {
    return lambda_provider_register(&devtools_host_ops, operations,
        &RadiantDevtoolsHostOps::open, &RadiantDevtoolsHostOps::tick, &RadiantDevtoolsHostOps::destroy);
}

RadiantDevtools* radiant_devtools_open(UiContext* ui, int fd, const char* source_path) {
    const RadiantDevtoolsHostOps* operations = lambda_provider_load(&devtools_host_ops);
    return operations ? operations->open(ui, fd, source_path) : nullptr;
}

bool radiant_devtools_tick(RadiantDevtools* inspector) {
    const RadiantDevtoolsHostOps* operations = lambda_provider_load(&devtools_host_ops);
    return operations && inspector && operations->tick(inspector);
}

void radiant_devtools_destroy(RadiantDevtools* inspector) {
    const RadiantDevtoolsHostOps* operations = lambda_provider_load(&devtools_host_ops);
    if (operations && inspector) operations->destroy(inspector);
}
