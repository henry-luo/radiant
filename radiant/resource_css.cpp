#include "resource_loaders.h"
#include "resource_observer.hpp"
#include "stylesheet_sync.hpp"
#include "network_integration.h"
#include "lambda/input/css/css_engine.hpp"
#include "lambda/input/css/css_parser.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lib/file_utils.h"
#include "lib/file.h"
#include "lib/mem_grow.hpp"
#include "lib/log.h"
#include "lib/str.h"
#include <string.h>

// Helper: Read file contents into a string
char* radiant_read_resource_file(const char* path, size_t* out_size) {
    if (!path) return NULL;
    char* content = NULL;
    if (!file_read_all(path, MEM_CAT_NETWORK, &content, out_size)) {
        log_error("resource_loaders: failed to read file: %s", path);
        return NULL;
    }
    return content;
}

struct CssResourceCompletion {
    NetworkResource* resource;
    DomDocument* document;
    bool loaded = false;
    ~CssResourceCompletion() {
        // completion follows stylesheet publication and runs through the host's pinned task queue.
        radiant_resource_complete(network_resource_owner(resource, document), loaded);
    }
};

void process_css_resource(NetworkResource* res, struct DomDocument* doc) {
    if (!res || (res->state != STATE_COMPLETED && res->state != STATE_CACHED) || !doc) return;
    DomElement* owner = network_resource_owner(res, doc);
    // A retired link cannot turn its downloaded sheet into a document-wide rule source.
    if (res->owner_node.address && !owner) return;
    CssResourceCompletion completion{res, doc};

    // the initial loader already parsed prefetched links; deliver completion without a second sheet.
    for (int i = 0; owner && i < doc->stylesheet_count; i++) {
        CssStylesheet* sheet = doc->stylesheets[i];
        if (sheet && sheet->owner_element == owner && !sheet->is_import_child &&
            sheet->origin_url && strcmp(sheet->origin_url, res->url) == 0) {
            completion.loaded = true;
            return;
        }
    }

    log_debug("network: processing CSS resource %s from %s", res->url, res->local_path);

    // read CSS content from local file
    size_t css_size = 0;
    lam::Temp<char> css_content(radiant_read_resource_file(res->local_path, &css_size));
    if (!css_content) {
        log_error("network: failed to read CSS file: %s", res->local_path);
        return;
    }

    // get or create CSS engine
    CssEngine* engine = NULL;
    if (res->manager && res->manager->css_engine) {
        engine = (CssEngine*)res->manager->css_engine;
    } else if (doc->document_pool) {
        // create temporary engine if none available
        engine = css_engine_create(doc->document_pool);
        if (!engine) {
            log_error("network: failed to create CSS engine");
            return;
        }
    } else {
        log_error("network: no CSS engine or pool available");
        return;
    }

    // parse the stylesheet
    engine->context.quirks_mode =
        is_quirks_mode((HtmlVersion)doc->html_version);
    CssStylesheet* sheet = css_parse_stylesheet(engine, css_content.get(), res->url);
    css_content.reset();  // content was copied by parser

    if (!sheet) {
        log_error("network: failed to parse CSS: %s", res->url);
        return;
    }

    log_debug("network: parsed CSS stylesheet with %zu rules", sheet->rule_count);

    // completion order must not override the link's position in the cascade.
    sheet->owner_element = owner;
    if (owner && owner->tag_name() && str_icmp_cstr(owner->tag_name(), "link") == 0) {
        // asynchronously inserted theme links keep their activation conditions;
        // a completed download alone does not make its rules applicable.
        const char* media = owner->get_attribute("media");
        sheet->media = media ? pool_strdup(doc->document_pool, media) : nullptr;
        const char* title = owner->get_attribute("title");
        sheet->disabled = owner->has_attribute("disabled") ||
            (title && *title && radiant_html_has_token(owner->get_attribute("rel"), "alternate"));
    }
    if (!radiant_append_document_stylesheet(doc, sheet)) {
        log_error("network: failed to add stylesheet to document");
        return;
    }

    // schedule reflow for entire document (CSS affects all elements)
    if (res->manager && doc->root) {
        resource_manager_schedule_reflow(res->manager, doc->root);
    }

    completion.loaded = true;
    log_debug("network: CSS resource processed successfully: %s", res->url);
}

