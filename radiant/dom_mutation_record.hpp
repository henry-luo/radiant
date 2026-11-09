#pragma once

#include "lambda/input/css/dom_element.hpp"

// Record native invalidation and retain the affected nodes for reconciliation.
void radiant_dom_record_mutation_detail(DomDocument* document, DomJsMutationKind kind,
                                        DomNode* target, DomNode* parent, uint32_t sequence,
                                        const char* attribute_name, CssPropertyCode presentation_property);
void radiant_dom_record_inline_stylesheet_mutation(DomDocument* document, DomJsMutationKind kind,
                                                   DomNode* target, DomNode* parent);
void radiant_dom_record_inline_stylesheet_owner(DomDocument* document, DomElement* style);
// The caller handles transaction deferral, then surrounds observer delivery with these calls.
void radiant_dom_mutation_begin(DomDocument* document, DomJsMutationKind kind,
                                DomNode* target, DomNode* parent, const char* attribute_name,
                                CssPropertyCode presentation_property);
void radiant_dom_mutation_finish(DomDocument* document, DomJsMutationKind kind);
void radiant_dom_notify_native(DomDocument* document, DomJsMutationKind kind,
                               DomNode* target, DomNode* parent,
                               const char* attribute_name, const char* old_value);
extern "C" bool dom_edit_transaction_defer_mutation(DomDocument* document, DomJsMutationKind kind,
                                                    DomNode* target, DomNode* parent,
                                                    const char* attribute_name, const char* old_value);

