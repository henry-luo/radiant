#pragma once

struct DomDocument;
struct DomElement;

// Reparse connected inline styles and preserve document stylesheet order.
bool radiant_sync_inline_stylesheet(DomElement* element);
void radiant_sync_mutated_inline_stylesheets(DomDocument* document);

struct CssStylesheet;
// loaded and inline sheets share document-tree cascade order.
bool radiant_append_document_stylesheet(DomDocument* doc, CssStylesheet* sheet);
