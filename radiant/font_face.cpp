#include "view.hpp"
#include "layout.hpp"
#include "radiant.hpp"
#include "document_font_load.hpp"
#include "network_integration.h"
#include "resource_loaders.h"
#include "lib/font/font.h"  // unified font module — font_face_register, font_family_exists
#include "lib/font/font_weight.h"
#include "lambda/input/css/css_style.hpp"
#include "lambda/input/css/css_parser.hpp"
#include "lambda/input/css/css_font_face.hpp"
#include "lambda/network/network_downloader.h"
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
#include "lib/strbuf.h"
#include "lib/arraylist.hpp"
#include "lib/mem_grow.hpp"

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

struct CssFontLoadFace {
    const CssFontFaceDescriptor* descriptor;
    int source_index;
    bool source_requested;
    bool loaded;
};

struct CssFontLoad {
    Pool* pool;
    CssFontLoadFace* faces;
    size_t count;
    size_t capacity;
    FontWeight weight;
};

static const char* font_load_family_name(Pool* pool, const CssValue* value) {
    if (!value || value->type != CSS_VALUE_TYPE_LIST)
        return css_font_family_name_from_value(value);
    StrBuf* buffer = strbuf_new();
    if (!buffer) return nullptr;
    for (int i = 0; i < value->data.list.count; i++) {
        const char* part = css_font_family_name_from_value(value->data.list.values[i]);
        if (!part || !*part) continue;
        if (buffer->length) strbuf_append_char(buffer, ' ');
        strbuf_append_str(buffer, part);
    }
    const char* family = buffer->length ? pool_dup_n(pool, buffer->str, buffer->length) : nullptr;
    strbuf_free(buffer);
    return family;
}

static bool font_load_covers_text(const CssFontFaceDescriptor* face, const char* text, size_t length) {
    if (!length) return false;
    if (!face->unicode_range_count) return true;
    for (size_t pos = 0; pos < length;) {
        uint32_t cp = 0;
        int bytes = str_utf8_decode(text + pos, length - pos, &cp);
        if (bytes < 1) {
            cp = 0xfffd;
            bytes = 1;
        }
        for (int i = 0; i < face->unicode_range_count; i++) {
            const CssFontFaceUnicodeRange& range = face->unicode_ranges[i];
            if (cp >= range.start_codepoint && cp <= range.end_codepoint) return true;
        }
        pos += (size_t)bytes;
    }
    return false;
}

static bool font_load_select_family(CssFontLoad* request,
    const lam::ArrayList<CssFontFaceDescriptor*>& faces, const char* family,
    FontSlant slant, const char* text, size_t text_len) {
    const CssFontFaceDescriptor* best = nullptr;
    int best_slant = 0;
    FontWeight best_weight = FONT_WEIGHT_NORMAL;
    for (size_t i = faces.size(); i > 0; i--) {
        const CssFontFaceDescriptor* candidate = faces[i - 1];
        if (str_icmp_cstr(candidate->family_name, family)) continue;
        int distance = font_face_slant_distance(radiant_font_slant_from_css(candidate->font_style), slant);
        FontWeight weight = radiant_font_weight_from_css(candidate->font_weight);
        if (!best || distance < best_slant || (distance == best_slant &&
            font_css_weight_is_better(request->weight, weight, best_weight))) {
            best = candidate;
            best_slant = distance;
            best_weight = weight;
        }
    }
    if (!best) return true;
    // Select the family/style first. The sample only filters that selected group.
    FontSlant matched_slant = radiant_font_slant_from_css(best->font_style);
    for (const CssFontFaceDescriptor* candidate : faces) {
        if (str_icmp_cstr(candidate->family_name, family) ||
            radiant_font_slant_from_css(candidate->font_style) != matched_slant ||
            radiant_font_weight_from_css(candidate->font_weight) != best_weight ||
            !font_load_covers_text(candidate, text, text_len)) continue;
        bool duplicate = false;
        for (size_t i = 0; !duplicate && i < request->count; i++)
            duplicate = request->faces[i].descriptor == candidate;
        if (duplicate) continue;
        if (!lam::pool_grow_array(request->pool, &request->faces, &request->capacity,
                request->count + 1, 4)) return false;
        request->faces[request->count++] = {candidate, 0, false, false};
    }
    return true;
}

CssFontLoad* radiant_css_font_load_begin(DomDocument* doc, const char* font, size_t font_len,
    const char* text, size_t text_len, CssFontLoadStatus* status) {
    *status = CSS_FONT_LOAD_FAILED;
    if (!doc) return nullptr;
    Pool* pool = pool_create();
    if (!pool) return nullptr;
    auto* request = (CssFontLoad*)pool_calloc(pool, sizeof(CssFontLoad));
    if (!request) {
        pool_destroy(pool);
        return nullptr;
    }
    request->pool = pool;
    CssDeclaration* declaration = css_parse_property_value_declaration("font", 4, font, font_len, pool);
    CssFontShorthandParts parts;
    if (!declaration || !css_parse_font_shorthand(declaration->value, &parts)) {
        *status = CSS_FONT_LOAD_SYNTAX;
        pool_destroy(pool);
        return nullptr;
    }
    const CssValue* families = css_font_shorthand_longhand(declaration->value, "font-family", pool);
    if (!families) {
        pool_destroy(pool);
        return nullptr;
    }
    request->weight = parts.weight && parts.weight->type == CSS_VALUE_TYPE_NUMBER
        ? (FontWeight)parts.weight->data.number.value
        : radiant_font_weight_from_css(parts.weight ? parts.weight->data.keyword : CSS_VALUE_NORMAL);
    FontSlant slant = radiant_font_slant_from_css(parts.style ? parts.style->data.keyword : CSS_VALUE_NORMAL);
    lam::ArrayList<CssFontFaceDescriptor*> faces(MEM_CAT_FONT, 0);
    lam::Temp<char> document_base(radiant_document_resource_base(doc, MEM_CAT_FONT));
    for (int i = 0; doc->stylesheets && i < doc->stylesheet_count; i++) {
        CssStylesheet* sheet = doc->stylesheets[i];
        if (!sheet || sheet->disabled) continue;
        int count = 0;
        CssFontFaceDescriptor** extracted = css_extract_font_faces(sheet,
            sheet->origin_url ? sheet->origin_url : document_base.get(), pool, &count);
        for (int j = 0; j < count; j++) {
            if (faces.push_back(extracted[j])) continue;
            pool_destroy(pool);
            return nullptr;
        }
    }
    for (int i = 0; i < css_value_comma_count(families); i++) {
        const char* family = font_load_family_name(pool, css_value_comma_at(families, i));
        if (!family || !font_load_select_family(request, faces, family, slant, text, text_len)) {
            pool_destroy(pool);
            return nullptr;
        }
    }
    *status = request->count ? CSS_FONT_LOAD_PENDING : CSS_FONT_LOAD_LOADED;
    return request;
}

static CssFontLoadStatus font_load_one_face(DomDocument* doc, UiContext* ui, CssFontLoad* request,
                                            CssFontLoadFace* item) {
    const CssFontFaceDescriptor* descriptor = item->descriptor;
    FontStyleDesc style = {};
    style.family = descriptor->family_name;
    style.weight = descriptor->font_weight == CSS_VALUE_AUTO
        ? request->weight : radiant_font_weight_from_css(descriptor->font_weight);
    style.slant = radiant_font_slant_from_css(descriptor->font_style);
    // Validation instantiates a scalable face; CSS size does not select its resource.
    style.size_px = css_font_size_keyword_px(CSS_VALUE_MEDIUM);
    for (; item->source_index < descriptor->src_count; item->source_index++, item->source_requested = false) {
        const CssFontFaceSrc& css_source = descriptor->src_urls[item->source_index];
        if (!css_source.url) continue;
        FontFaceSource source = {};
        lam::Temp<char> path;
        if (css_source.is_local) {
            source.local_name = css_source.url;
        } else {
            if (!document_dependency_admits(doc, css_source.url)) continue;
            if (!strncmp(css_source.url, "data:", 5)) {
                source.path = css_source.url;
            } else if (radiant_url_is_http(css_source.url)) {
                if (!radiant_is_supported_web_font_source(css_source.url, css_source.format)) continue;
                if (!doc->resource_manager) {
                    network_downloader_init_shared();
                    radiant_init_network_support(doc, nullptr, nullptr);
                }
                if (!doc->resource_manager) continue;
                if (!item->source_requested) {
                    resource_manager_load(doc->resource_manager, css_source.url, RESOURCE_FONT, PRIORITY_HIGH, nullptr);
                    item->source_requested = true;
                }
                bool pending = false;
                path = lam::Temp<char>(resource_manager_copy_ready_resource_path(
                    doc->resource_manager, css_source.url, &pending));
                if (pending) return CSS_FONT_LOAD_PENDING;
                // Publish the decoded source through the renderer's own reflow path.
                if (!radiant_install_downloaded_font_face(ui, doc->resource_manager, path.get(), descriptor)) continue;
                item->loaded = true;
                return CSS_FONT_LOAD_LOADED;
            } else if (!radiant_url_is_http(doc->url ? url_get_href(doc->url) : nullptr)) {
                lam::Temp<char> base(radiant_document_resource_base(doc, MEM_CAT_FONT));
                path = lam::Temp<char>(radiant_resolve_resource_path(css_source.url, base.get(), true, MEM_CAT_FONT));
                source.path = path.get();
            }
        }
        FontHandle* decoded = font_face_load_source(ui->font_ctx, &source, &style);
        if (!decoded) continue;
        font_handle_release(decoded);
        item->loaded = true;
        return CSS_FONT_LOAD_LOADED;
    }
    return CSS_FONT_LOAD_FAILED;
}

CssFontLoadStatus radiant_css_font_load_poll(DomDocument* doc, UiContext* ui, CssFontLoad* request) {
    if (!request || !doc || !ui || !ui->font_ctx) return CSS_FONT_LOAD_FAILED;
    bool pending = false;
    for (size_t i = 0; i < request->count; i++) {
        CssFontLoadFace* face = &request->faces[i];
        if (face->loaded) continue;
        CssFontLoadStatus status = font_load_one_face(doc, ui, request, face);
        if (status == CSS_FONT_LOAD_FAILED) return status;
        pending |= status == CSS_FONT_LOAD_PENDING;
    }
    return pending ? CSS_FONT_LOAD_PENDING : CSS_FONT_LOAD_LOADED;
}

bool radiant_css_font_load_face(const CssFontLoad* request, size_t index, CssFontLoadFaceInfo* result) {
    if (!request || index >= request->count || !result || !request->faces[index].loaded) return false;
    const CssFontFaceDescriptor* face = request->faces[index].descriptor;
    const CssEnumInfo* style = css_enum_info(face->font_style);
    const char* weight = "normal";
    FontWeight numeric = radiant_font_weight_from_css(face->font_weight);
    const char* const weights[] = {"100","200","300","400","500","600","700","800","900"};
    if (face->font_weight == CSS_VALUE_BOLD) weight = "bold";
    else if (face->font_weight != CSS_VALUE_NORMAL && face->font_weight != CSS_VALUE_AUTO &&
        numeric >= FONT_WEIGHT_THIN && numeric <= FONT_WEIGHT_BLACK)
        weight = weights[(numeric / 100) - 1];
    *result = {face->family_name, style ? style->name : "normal", weight};
    return true;
}

void radiant_css_font_load_destroy(CssFontLoad* request) {
    if (request) pool_destroy(request->pool);
}
