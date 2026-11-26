#pragma once

#include "lib/font/css_font_load.h"

struct DomDocument;
struct UiContext;

CssFontLoad* radiant_css_font_load_begin(DomDocument* doc, const char* font, size_t font_len,
    const char* text, size_t text_len, CssFontLoadStatus* status);
CssFontLoadStatus radiant_css_font_load_poll(DomDocument* doc, UiContext* ui, CssFontLoad* request);
bool radiant_css_font_load_face(const CssFontLoad* request, size_t index, CssFontLoadFaceInfo* result);
void radiant_css_font_load_destroy(CssFontLoad* request);
