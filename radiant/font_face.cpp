#include "view.hpp"
#include "layout.hpp"
#include "radiant.hpp"
#include "lib/font/font.h"  // unified font module — font_face_register, font_family_exists
#include "lambda/input/css/css_style.hpp"
#include "lambda/input/css/css_font_face.hpp"
#include "lambda/network/network_resource_manager.h"
extern "C" {
#include "lib/url.h"
#include "lib/memtrack.h"
#include "lib/alloca.h"
#include "lib/str.h"
}
#include <string.h>
#include <stdlib.h>
#include "lib/file.h"

bool radiant_is_supported_web_font_source(const char* url, const char* format) {
    if (format && *format) {
        if (str_icmp_cstr(format, "woff2") == 0 ||
            str_icmp_cstr(format, "woff") == 0 ||
            str_icmp_cstr(format, "truetype") == 0 ||
            str_icmp_cstr(format, "opentype") == 0 ||
            str_icmp_cstr(format, "ttf") == 0 ||
            str_icmp_cstr(format, "otf") == 0) {
            return true;
        }
        return false;
    }

    return url_text_path_has_ext_ci(url, "woff2") ||
           url_text_path_has_ext_ci(url, "woff") ||
           url_text_path_has_ext_ci(url, "ttf") ||
           url_text_path_has_ext_ci(url, "otf") ||
           url_text_path_has_ext_ci(url, "ttc");
}

static bool resolve_font_source_path(char** source, const char* base_path,
                                     NetworkResourceManager* resource_manager,
                                     const char* format) {
    if (!source || !*source ||
        strncmp(*source, "data:", 5) == 0) {
        return false;
    }

    char* replacement = nullptr;
    bool pending = false;
    if (radiant_url_is_http(*source)) {
        if (!radiant_is_supported_web_font_source(*source, format)) return false;
        // keep downloaded URLs in the original src list; a later local-only
        // registration otherwise shadows the completed network face.
        replacement = resource_manager_copy_ready_resource_path(resource_manager, *source, &pending);
    } else {
        if (!base_path || file_exists(*source)) return false;
        // fonts share the same local resource roots as stylesheets and images;
        // a missing server-root URL must not silently select system-font metrics.
        lam::Temp<char> resolved(radiant_resolve_resource_path(
            *source, base_path, true, MEM_CAT_FONT));
        if (resolved && file_exists(resolved.get())) {
            replacement = resolved.release();
        } else {
            char relative[2048];
            if (!radiant_resolve_layout_relative_resource_path(
                    *source, base_path, relative, sizeof(relative))) return false;
            replacement = mem_strdup(relative, MEM_CAT_FONT);
        }
    }
    if (!replacement) return pending;
    lam::Temp<char> previous(*source);  // the replacement takes the slot
    *source = replacement;
    return false;
}

// Text flow logging categories
log_category_t* font_log = NULL;
log_category_t* text_log = NULL;
log_category_t* layout_log = NULL;

// Initialize logging categories for text flow
void init_text_flow_logging(void) {
    font_log = log_get_category("radiant.font");
    text_log = log_get_category("radiant.text");
    layout_log = log_get_category("radiant.layout");

    if (!font_log || !text_log || !layout_log) {
        log_warn("Failed to initialize text flow logging categories");
    } else {
        log_info("Text flow logging categories initialized");
    }
}

static void register_css_font_face(UiContext* ui, const CssFontFaceDescriptor* css,
                                   bool has_pending_sources = false) {
    if (!ui || !ui->font_ctx || !css->family_name) return;

    // FontContext copies the resource synchronously, so the CSS descriptor
    // can supply borrowed font URLs and names without a second retained copy.
    FontFaceSource* sources = LAMBDA_ALLOCA(css->src_count > 0 ? css->src_count : 1, FontFaceSource);
    int source_count = 0;
    for (int i = 0; css->src_urls && i < css->src_count; i++) {
        const CssFontFaceSrc* src = &css->src_urls[i];
        if (!src->url) continue;
        FontFaceSource& source = sources[source_count++];
        source = {};
        if (src->is_local) source.local_name = src->url;
        else source.path = src->url;
        source.format = src->format;
    }
    if (source_count == 0 && css->src_url) {
        sources[source_count++] = {};
        sources[0].path = css->src_url;
    }

    lam::Temp<FontFaceUnicodeRange> ranges;
    if (css->unicode_range_count > 0 && css->unicode_ranges) {
        ranges = lam::temp_array_zero<FontFaceUnicodeRange>(
            (size_t)css->unicode_range_count, MEM_CAT_FONT);
        if (!ranges) return;
        for (int i = 0; i < css->unicode_range_count; i++) {
            ranges[i].start_codepoint = css->unicode_ranges[i].start_codepoint;
            ranges[i].end_codepoint = css->unicode_ranges[i].end_codepoint;
        }
    }

    FontFaceDesc face = {};
    face.family = css->family_name;
    face.weight = css->font_weight == CSS_VALUE_AUTO
        ? FONT_WEIGHT_AUTO : radiant_font_weight_from_css(css->font_weight);
    face.slant = radiant_font_slant_from_css(css->font_style);
    face.sources = sources;
    face.source_count = source_count;
    face.unicode_ranges = ranges.get();
    face.unicode_range_count = ranges ? css->unicode_range_count : 0;
    face.has_pending_sources = has_pending_sources;
    font_face_register(ui->font_ctx, &face);
}

// CSS @font-face parsing integration - uses css_font_face.hpp module
void parse_font_face_rule(LayoutContext* lycon, void* rule) {
    if (!lycon || !rule) {
        clog_debug(font_log, "parse_font_face_rule: invalid parameters");
        return;
    }

    CssRule* css_rule = (CssRule*)rule;
    if (css_rule->type != CSS_RULE_FONT_FACE) {
        clog_debug(font_log, "parse_font_face_rule: not a font-face rule");
        return;
    }

    const char* content = css_rule->data.generic_rule.content;
    if (!content) {
        clog_warn(font_log, "parse_font_face_rule: no content in rule");
        return;
    }

    // Resource ownership stays explicit: this helper always returns a font-owned copy.
    lam::Temp<char> owned_base_path(radiant_document_resource_base(
        lycon->doc, MEM_CAT_FONT));
    const char* base_path = owned_base_path.get();

    // Parse using CSS module
    CssFontFaceDescriptor* css_desc = css_parse_font_face_content(content, nullptr);
    if (!css_desc) return;

    css_font_face_resolve_sources(css_desc, base_path, nullptr);

    register_css_font_face(lycon->ui_context, css_desc);
    css_font_face_descriptor_free(css_desc);
}

// Process all @font-face rules from a stylesheet - uses css_font_face.hpp module
void process_font_face_rules_from_stylesheet(UiContext* uicon, CssStylesheet* stylesheet,
        const char* base_path, bool data_only, NetworkResourceManager* resource_manager) {
    if (!uicon || !stylesheet) {
        return;
    }

    clog_info(font_log, "Processing @font-face rules from stylesheet (base: %s)",
              base_path ? base_path : "(none)");

    int count = 0;
    CssFontFaceDescriptor** css_descs = css_extract_font_faces(stylesheet, base_path, nullptr, &count);
    // extraction can allocate an array even when every descriptor is rejected (D5.5.1).
    lam::Temp<CssFontFaceDescriptor*> descriptor_array(css_descs);

    if (!css_descs || count == 0) {
        clog_debug(font_log, "No @font-face rules found");
        return;
    }

    for (int i = 0; i < count; i++) {
        CssFontFaceDescriptor* css_desc = css_descs[i];
        if (!css_desc) continue;
        bool has_pending_sources = false;

        // only the ordered list retains format hints; its scalar compatibility
        // URL must not reintroduce an unsupported source after list filtering.
        has_pending_sources |= resolve_font_source_path(&css_desc->src_url, base_path,
            !data_only && css_desc->src_count == 0 ? resource_manager : nullptr, nullptr);
        if (css_desc->src_urls) {
            for (int j = 0; j < css_desc->src_count; j++) {
                if (!css_desc->src_urls[j].is_local)
                    has_pending_sources |= resolve_font_source_path(&css_desc->src_urls[j].url, base_path,
                        data_only ? nullptr : resource_manager, css_desc->src_urls[j].format);
            }
        }

        // Remote web fonts are discovered by the network resource manager and
        // installed when their downloads complete; synchronously downloading
        // every @font-face source here blocks large docs before layout starts.
        if (css_desc->src_urls) {
            for (int j = 0; j < css_desc->src_count; j++) {
                if (!css_desc->src_urls[j].is_local && radiant_url_is_http(css_desc->src_urls[j].url)) {
                    if (!radiant_is_supported_web_font_source(
                            css_desc->src_urls[j].url, css_desc->src_urls[j].format)) {
                        clog_debug(font_log, "Skipping unsupported remote font source: %s (format: %s)",
                                   css_desc->src_urls[j].url,
                                   css_desc->src_urls[j].format ? css_desc->src_urls[j].format : "?");
                    }
                    lam::Temp<char> dropped(css_desc->src_urls[j].url);
                    css_desc->src_urls[j].url = nullptr;
                }
            }
        }
        if (radiant_url_is_http(css_desc->src_url)) {
            lam::Temp<char> dropped(css_desc->src_url);
            css_desc->src_url = nullptr;
        }

        // SVG image documents can use embedded font bytes without fetching external resources.
        if (data_only) {
            if (css_desc->src_url && strncmp(css_desc->src_url, "data:", 5) != 0) {
                lam::Temp<char> dropped(css_desc->src_url);
                css_desc->src_url = nullptr;
            }
            for (int j = 0; css_desc->src_urls && j < css_desc->src_count; j++) {
                char*& source = css_desc->src_urls[j].url;
                if (source && !css_desc->src_urls[j].is_local && strncmp(source, "data:", 5) != 0) {
                    lam::Temp<char> dropped(source); source = nullptr;
                }
            }
        }

        bool has_loadable_source = css_desc->src_url || css_desc->src_local;
        if (css_desc->src_urls) {
            for (int j = 0; j < css_desc->src_count; j++) {
                if (css_desc->src_urls[j].url) {
                    has_loadable_source = true;
                    break;
                }
            }
        }

        // Skip fonts without any loadable source
        if (!has_loadable_source) {
            clog_debug(font_log, "Skipping @font-face '%s': no local source available",
                       css_desc->family_name ? css_desc->family_name : "(unnamed)");
            css_font_face_descriptor_free(css_desc);
            continue;
        }

        register_css_font_face(uicon, css_desc, has_pending_sources);
        css_font_face_descriptor_free(css_desc);
    }

    clog_info(font_log, "Registered %d @font-face descriptors", count);
}

// Helper function to process all @font-face rules from a document's stylesheets
void process_document_font_faces(UiContext* uicon, DomDocument* doc) {
    if (!uicon || !doc) return;
    if (doc->font_faces_processed) return;
    // Geometry can be queried while load scripts run; make this idempotent so
    // that early font readiness and the normal final-load phase share one registry.
    if (!doc->stylesheets || doc->stylesheet_count == 0) {
        doc->font_faces_processed = true;
        return;
    }

    // Resource ownership stays explicit: this helper always returns a font-owned copy.
    lam::Temp<char> owned_doc_base_path(radiant_document_resource_base(
        doc, MEM_CAT_FONT));
    const char* doc_base_path = owned_doc_base_path.get();

    for (int i = 0; i < doc->stylesheet_count; i++) {
        CssStylesheet* stylesheet = doc->stylesheets[i];
        if (!stylesheet) continue;

        // An external stylesheet owns the base for its relative font URLs.
        const char* base_path = doc_base_path;
        lam::Temp<char> stylesheet_path(stylesheet->origin_url
            ? radiant_resolve_resource_path(stylesheet->origin_url,
                doc_base_path, false, MEM_CAT_FONT) : nullptr);
        if (stylesheet_path) base_path = stylesheet_path.get();

        process_font_face_rules_from_stylesheet(uicon, stylesheet, base_path, false,
            doc->resource_manager);
    }
    doc->font_faces_processed = true;
}
