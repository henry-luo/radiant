#pragma once

#include "event.hpp"

// Native hit testing reads the event context; it does not dispatch script.
void target_html_doc(EventContext* context, ViewTree* tree);
void target_block_view(EventContext* context, ViewBlock* block);
void target_inline_view(EventContext* context, ViewSpan* span);
void target_text_view(EventContext* context, ViewText* text);
bool radiant_hit_rich_editable_host(View* view);
DocState* event_context_target_state(EventContext* context);
DocState* event_view_owner_state(View* view);
