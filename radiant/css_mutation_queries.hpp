#pragma once

struct CssRule;
struct CssSelector;
struct CssStylesheet;
struct DomDocument;

typedef bool (*RadiantCssRulePredicate)(CssRule* rule, void* context);
typedef bool (*RadiantCssSelectorPredicate)(CssSelector* selector, void* context);

bool radiant_css_selector_mentions_mutation_attribute(CssSelector* selector, const char* attribute);
bool radiant_css_stylesheet_tree_has_match(CssStylesheet* stylesheet, RadiantCssRulePredicate predicate, void* context);
bool radiant_css_rule_selector_has_match(CssRule* rule, RadiantCssSelectorPredicate predicate, void* context);
bool radiant_css_document_rule_tree_has_match(DomDocument* document, RadiantCssRulePredicate predicate, void* context);
bool radiant_css_document_has_relational_mutation_attribute_dependency(DomDocument* document, const char* name);
extern "C" bool dom_engine_attribute_has_relational_css_dependency(DomDocument* document, const char* name);
