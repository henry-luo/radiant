#include "radiant/event_host.hpp"
#include "radiant/event_navigation_host.hpp"
#include "radiant/event.hpp"
#include "radiant/radiant.hpp"
#include "radiant/render.hpp"
#include "lambda/input/css/dom_element.hpp"

int main() {
    // A native client has no controller and no script-owned loads.
    if (radiant_event_host_dispatch(nullptr, nullptr, nullptr)) return 1;
    if (!radiant_event_host_allow_frame_reflow()) return 2;
    if (!radiant_event_navigation_poll_loads(nullptr)) return 3;
    UiContext ui = {};
    if (ui_context_init(&ui, true, 1.0f) != 0) return 4;
    ui.viewport_width = 320;
    ui.viewport_height = 200;
    DocumentJsHostConfig host = {};
    host.ui_context = &ui;
    host.resource_policy = INPUT_RESOURCE_LOCAL_ONLY;
    const char* html = "<!doctype html><style>html,body{margin:0}</style>"
        "<div id=box style='position:absolute;left:0;top:0;width:40px;height:40px;background:red'></div>"
        "<svg style='position:absolute;left:50px;top:0' width=40 height=40>"
        "<rect id=shape x=0 y=0 width=40 height=40 fill=blue></rect></svg>";
    DomDocument* document = load_html_source_doc(html, 320, 200, &host);
    if (!document || !document->root) return 5;
    ui.document = lam::up(document);
    layout_html_doc(&ui, document, false);
    DomElement* box = dom_find_element_by_id(document->root->as_element(), "box");
    DomElement* shape = dom_find_element_by_id(document->root->as_element(), "shape");
    if (!box || radiant_document_element_from_point(document, 10.0f, 10.0f) != box) return 6;
    if (!shape || radiant_document_element_from_point(document, 60.0f, 10.0f) != shape) return 7;
    ui.document = nullptr;
    free_document(document);
    if (!ui_context_cleanup(&ui)) return 8;
    return 0;
}
