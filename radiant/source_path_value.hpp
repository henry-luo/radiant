#pragma once

#include "source_path.hpp"
#include "lambda/core/data.hpp"

class MarkBuilder;

// Convert native source positions and selections through the input-owned Mark API.
Item source_pos_to_item(MarkBuilder& builder, const SourcePosC* position);
Item source_text_selection_to_item(MarkBuilder& builder, const SourcePosC* anchor, const SourcePosC* head);
Item source_node_selection_to_item(MarkBuilder& builder, const SourcePathC* path);
bool source_pos_from_item(Item value, SourcePosC* position);
bool source_path_from_item(Item value, SourcePathC* path);
