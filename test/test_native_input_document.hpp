#pragma once

#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/input.hpp"

struct NativeInputDocument {
    Pool* pool;
    Input* input;
    DomDocument* document;
    NativeInputDocument() : pool(pool_create()), input(pool ? Input::create(pool) : nullptr),
        document(input ? dom_document_create(input) : nullptr) {}
    ~NativeInputDocument() {
        if (document) dom_document_destroy(document);
        if (pool) pool_destroy(pool);
    }
};
