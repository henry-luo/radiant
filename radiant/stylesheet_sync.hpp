#pragma once

struct DomDocument;
struct DomElement;

// Reparse connected inline styles and preserve document stylesheet order.
bool radiant_sync_inline_stylesheet(DomElement* element);
void radiant_sync_mutated_inline_stylesheets(DomDocument* document);

struct CssStylesheet;
CssStylesheet* radiant_shadow_inline_stylesheet(DomElement* owner);
CssStylesheet* radiant_next_shadow_stylesheet(DomElement* root, DomElement* after);
// loaded and inline sheets share document-tree cascade order.
bool radiant_append_document_stylesheet(DomDocument* doc, CssStylesheet* sheet);
