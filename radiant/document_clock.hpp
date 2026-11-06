#pragma once
struct DomDocument;

// The optional host supplies realm time; a standalone engine uses monotonic time.
struct RadiantDocumentClockOps {
    bool (*read)(DomDocument* document, double* now_ms);
};
bool radiant_document_clock_register(const RadiantDocumentClockOps* operations);

// document-realm monotonic milliseconds; returns true for a virtual clock.
bool radiant_document_clock(DomDocument* document, double* now_ms);
