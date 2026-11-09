#include "document_clock.hpp"
#include "lib/provider.hpp"
#include <uv.h>

static const RadiantDocumentClockOps* document_clock_ops = nullptr;

bool radiant_document_clock_register(const RadiantDocumentClockOps* operations) {
    return lambda_provider_register(&document_clock_ops, operations, &RadiantDocumentClockOps::read);
}

bool radiant_document_clock(DomDocument* document, double* now_ms) {
    const RadiantDocumentClockOps* operations = lambda_provider_load(&document_clock_ops);
    if (operations) return operations->read(document, now_ms);
    // A native-only consumer has no script realm or virtual event loop.
    if (now_ms) *now_ms = (double)uv_hrtime() / 1e6;
    return false;
}
