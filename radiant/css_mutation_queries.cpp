#include "css_mutation_queries.hpp"
#include "lambda/input/css/dom_element.hpp"
#include "lambda/input/css/css_parser.hpp"
#include "lib/str.h"

static bool dom_js_simple_selector_mentions_mutation_attribute(
        CssSimpleSelector* simple, const char* attribute);

bool radiant_css_selector_mentions_mutation_attribute(
        CssSelector* selector, const char* attribute) {
    if (!selector) return false;
    for (size_t i = 0; i < selector->compound_selector_count; i++) {
        CssCompoundSelector* compound = selector->compound_selectors[i];
        if (!compound) continue;
        for (size_t s = 0; s < compound->simple_selector_count; s++) {
            if (dom_js_simple_selector_mentions_mutation_attribute(
                    compound->simple_selectors[s], attribute)) {
                return true;
            }
        }
    }
    return false;
}

static bool dom_js_simple_selector_mentions_mutation_attribute(
        CssSimpleSelector* simple, const char* attribute) {
    if (!simple) return false;
    const char* selected_attribute = simple->type == CSS_SELECTOR_TYPE_CLASS ? "class" :
        simple->type == CSS_SELECTOR_TYPE_ID ? "id" :
        (simple->type >= CSS_SELECTOR_ATTR_EXACT && simple->type <= CSS_SELECTOR_ATTR_CASE_SENSITIVE)
            ? simple->attribute.name : nullptr;
    if (attribute && selected_attribute && str_icmp_cstr(selected_attribute, attribute) == 0) return true;
    for (size_t i = 0; i < simple->function_selector_count; i++) {
        if (radiant_css_selector_mentions_mutation_attribute(
                simple->function_selectors[i], attribute)) {
            return true;
        }
    }
    return false;
}

static bool dom_js_selector_has_relational_mutation_attribute_dependency(
        CssSelector* selector, const char* attribute);

static bool dom_js_simple_selector_has_relational_mutation_attribute_dependency(
        CssSimpleSelector* simple, const char* attribute) {
    if (!simple) return false;
    if (simple->type == CSS_SELECTOR_PSEUDO_HAS) {
        for (size_t i = 0; i < simple->function_selector_count; i++) {
            if (radiant_css_selector_mentions_mutation_attribute(
                    simple->function_selectors[i], attribute)) {
                return true;
            }
        }
    }
    for (size_t i = 0; i < simple->function_selector_count; i++) {
        if (dom_js_selector_has_relational_mutation_attribute_dependency(
                simple->function_selectors[i], attribute)) {
            return true;
        }
    }
    return false;
}

static bool dom_js_selector_has_relational_mutation_attribute_dependency(
        CssSelector* selector, const char* attribute) {
    if (!selector) return false;
    for (size_t i = 0; i < selector->compound_selector_count; i++) {
        CssCompoundSelector* compound = selector->compound_selectors[i];
        if (!compound) continue;
        for (size_t s = 0; s < compound->simple_selector_count; s++) {
            if (dom_js_simple_selector_has_relational_mutation_attribute_dependency(
                    compound->simple_selectors[s], attribute)) {
                return true;
            }
        }
    }
    return false;
}

static bool dom_js_rule_tree_has_match(CssRule* rule,
                                       RadiantCssRulePredicate predicate,
                                       void* context) {
    if (!rule) return false;
    if (predicate && predicate(rule, context)) return true;
    if (rule->type == CSS_RULE_STYLE ||
        rule->type == CSS_RULE_NESTING ||
        rule->type == CSS_RULE_NESTED_DECLARATIONS) {
        for (size_t i = 0; i < rule->data.style_rule.nested_rule_count; i++) {
            if (dom_js_rule_tree_has_match(rule->data.style_rule.nested_rules[i],
                                           predicate, context)) {
                return true;
            }
        }
    } else if (rule->type == CSS_RULE_MEDIA ||
               rule->type == CSS_RULE_SUPPORTS ||
               rule->type == CSS_RULE_CONTAINER ||
               rule->type == CSS_RULE_SCOPE ||
               rule->type == CSS_RULE_LAYER) {
        for (size_t i = 0; i < rule->data.conditional_rule.rule_count; i++) {
            if (dom_js_rule_tree_has_match(rule->data.conditional_rule.rules[i],
                                           predicate, context)) {
                return true;
            }
        }
    }
    return false;
}

bool radiant_css_stylesheet_tree_has_match(CssStylesheet* stylesheet,
                                             RadiantCssRulePredicate predicate,
                                             void* context) {
    if (!stylesheet || stylesheet->disabled) return false;
    for (size_t i = 0; i < stylesheet->rule_count; i++) {
        if (dom_js_rule_tree_has_match(stylesheet->rules[i], predicate, context)) {
            return true;
        }
    }
    for (size_t i = 0; i < stylesheet->imported_count; i++) {
        if (radiant_css_stylesheet_tree_has_match(stylesheet->imported_stylesheets[i],
                                             predicate, context)) {
            return true;
        }
    }
    return false;
}

bool radiant_css_rule_selector_has_match(CssRule* rule,
        RadiantCssSelectorPredicate predicate, void* context) {
    if (!rule || (rule->type != CSS_RULE_STYLE && rule->type != CSS_RULE_NESTING &&
            rule->type != CSS_RULE_NESTED_DECLARATIONS)) return false;
    CssSelectorGroup* group = rule->data.style_rule.selector_group;
    if (group) {
        for (size_t i = 0; i < group->selector_count; i++) {
            if (predicate(group->selectors[i], context)) return true;
        }
    }
    return predicate(rule->data.style_rule.selector, context);
}

bool radiant_css_document_rule_tree_has_match(DomDocument* doc,
                                                RadiantCssRulePredicate predicate, void* context) {
    if (!doc || !doc->stylesheets) return false;
    for (int i = 0; i < doc->stylesheet_count; i++) {
        if (radiant_css_stylesheet_tree_has_match(doc->stylesheets[i],
                predicate, context)) return true;
    }
    return false;
}

static bool dom_js_selector_has_attribute_dependency(CssSelector* selector, void* context) {
    return dom_js_selector_has_relational_mutation_attribute_dependency(
        selector, *(const char**)context);
}

static bool dom_js_rule_has_relational_mutation_attribute_dependency(
        CssRule* rule, void* context) {
    return radiant_css_rule_selector_has_match(rule, dom_js_selector_has_attribute_dependency, context);
}

bool radiant_css_document_has_relational_mutation_attribute_dependency(
        DomDocument* doc, const char* attribute) {
    return radiant_css_document_rule_tree_has_match(doc,
        dom_js_rule_has_relational_mutation_attribute_dependency, &attribute);
}

extern "C" bool dom_engine_attribute_has_relational_css_dependency(DomDocument* doc, const char* name) {
    if (!doc || !name || !doc->stylesheets || doc->stylesheet_count <= 0) return true;
    return radiant_css_document_has_relational_mutation_attribute_dependency(doc, name);
}

