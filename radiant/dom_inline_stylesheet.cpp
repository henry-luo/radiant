#include "dom_mutation_record.hpp"
#include "view.hpp"
#include "lambda/input/css/dom_lifecycle.hpp"
#include "lib/str.h"
#include "lib/mem_grow.hpp"
#include "lib/log.h"

// Keep stylesheet mutation ownership available without the full mutation host.
void radiant_dom_record_inline_stylesheet_owner(DomDocument* doc,
                                               DomElement* style) {
    if (!doc || !style) return;
    DomJsRuntime* js = &doc->js;
    for (int i = 0; i < js->inline_stylesheet_mutation_count; i++) {
        if (js->inline_stylesheet_mutations[i] == style) return;
    }
    if (js->inline_stylesheet_mutation_count >=
        js->inline_stylesheet_mutation_capacity) {
        if (!lam::pool_copy_grow_array(doc->document_pool,
                                       &js->inline_stylesheet_mutations,
                                       &js->inline_stylesheet_mutation_capacity,
                                       js->inline_stylesheet_mutation_count,
                                       js->inline_stylesheet_mutation_count + 1, 4, false)) {
            log_error("dom mutation: failed to retain inline stylesheet owner");
            return;
        }
    }
    js->inline_stylesheet_mutations[js->inline_stylesheet_mutation_count++] = style;
}

static void dom_record_inline_stylesheets_in_subtree(DomDocument* doc,
                                                      DomNode* node) {
    if (!node) return;
    if (node->is_element()) {
        DomElement* element = static_cast<DomElement*>(node);
        if (element->tag_name() && str_icmp_cstr(element->tag_name(), "style") == 0) {
            radiant_dom_record_inline_stylesheet_owner(doc, element);
        }
        if (dom_element_is_html_tag(element, MARKUP_NAME_TEMPLATE)) return;
        if (element->shadow_root_element())
            dom_record_inline_stylesheets_in_subtree(doc, element->shadow_root_element());
        for (DomNode* child = dom_source_child(element, true); child; child = dom_source_sibling(child, true)) {
            dom_record_inline_stylesheets_in_subtree(doc, child);
        }
    }
}

static DomElement* dom_inline_stylesheet_mutation_owner(DomNode* target,
                                                         DomNode* parent) {
    DomNode* candidates[] = {target, parent};
    for (DomNode* node : candidates) {
        if (!node) continue;
        DomElement* element = node->is_element()
            ? static_cast<DomElement*>(node)
            : (node->parent && node->parent->is_element()
                ? static_cast<DomElement*>(node->parent) : nullptr);
        if (element && element->tag_name() && str_icmp_cstr(element->tag_name(), "style") == 0) {
            return element;
        }
    }
    return nullptr;
}

void radiant_dom_record_inline_stylesheet_mutation(DomDocument* doc,
                                                   DomJsMutationKind kind,
                                                   DomNode* target,
                                                   DomNode* parent) {
    DomJsRuntime* js = doc ? &doc->js : nullptr;
    if (!js || (kind != DOM_JS_MUTATION_CHILD_INSERT &&
                kind != DOM_JS_MUTATION_CHILD_REMOVE &&
                kind != DOM_JS_MUTATION_TREE_REPLACE &&
                kind != DOM_JS_MUTATION_TEXT)) {
        return;
    }
    if (kind == DOM_JS_MUTATION_CHILD_INSERT ||
        kind == DOM_JS_MUTATION_CHILD_REMOVE ||
        kind == DOM_JS_MUTATION_TREE_REPLACE) {
        // A subtree replacement can introduce several <style> descendants.
        dom_record_inline_stylesheets_in_subtree(doc, target);
    }
    DomElement* style = dom_inline_stylesheet_mutation_owner(target, parent);
    if (!style) return;
    radiant_dom_record_inline_stylesheet_owner(doc, style);
}

