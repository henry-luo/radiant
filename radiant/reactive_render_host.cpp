#include "reactive_render_host.hpp"
#include "lambda/core/value.hpp"
#include "lib/provider.hpp"

static const RadiantReactiveRenderHostOps* reactive_render_host_ops = nullptr;

bool radiant_reactive_render_host_register(const RadiantReactiveRenderHostOps* operations) {
    return lambda_provider_register(&reactive_render_host_ops, operations,
        &RadiantReactiveRenderHostOps::document_root, &RadiantReactiveRenderHostOps::result,
        &RadiantReactiveRenderHostOps::equal, &RadiantReactiveRenderHostOps::set_metadata);
}

bool radiant_reactive_render_host_available() {
    return lambda_provider_load(&reactive_render_host_ops) != nullptr;
}

Item radiant_reactive_render_document_root() {
    const RadiantReactiveRenderHostOps* operations = lambda_provider_load(&reactive_render_host_ops);
    return operations ? operations->document_root() : ItemNull;
}

Item radiant_reactive_render_result(Item source, const char* template_ref) {
    const RadiantReactiveRenderHostOps* operations = lambda_provider_load(&reactive_render_host_ops);
    return operations ? operations->result(source, template_ref) : ItemNull;
}

bool radiant_reactive_render_equal(Item left, Item right) {
    const RadiantReactiveRenderHostOps* operations = lambda_provider_load(&reactive_render_host_ops);
    return operations && operations->equal(left, right);
}

bool radiant_reactive_render_set_metadata(DomElement* element, const char* name, const char* value) {
    const RadiantReactiveRenderHostOps* operations = lambda_provider_load(&reactive_render_host_ops);
    return operations && operations->set_metadata(element, name, value);
}
