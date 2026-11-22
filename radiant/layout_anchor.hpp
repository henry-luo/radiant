#pragma once

#include "layout.hpp"

struct LayoutAnchorSnapshot;

struct LayoutAnchorPass {
    LayoutAnchorSnapshot* previous = nullptr;
    LayoutAnchorSnapshot* current = nullptr;
    size_t rounds = 0;

    ~LayoutAnchorPass();
    bool settle(LayoutContext* lycon, View* root);
    void begin(LayoutContext* lycon) const;
};

struct LayoutAnchorFrame {
    LayoutAnchorFrame(LayoutContext* lycon, ViewBlock* block, ViewBlock* containing,
                      LayoutContainingBlock original);
    ~LayoutAnchorFrame();

    LayoutContext* lycon;
    LayoutAnchorFrame* previous;
    ViewBlock* block;
    ViewBlock* containing;
    LayoutContainingBlock box;
    LayoutAxisPair<CssAnchorAreaTrack> tracks{};
    RdtLogicalPoint origin{};
    RdtLogicalPoint center{};
    DomElement* default_element = nullptr;
    uint8_t scroll_axes = 0;
    bool active = false;
    bool has_default = false;
    bool area = false;
};

bool layout_resolve_anchor_query(LayoutContext* lycon, uintptr_t property,
                                 const CssAnchorQuery& query, double* result);
void layout_apply_anchor_alignment(LayoutContext* lycon, LayoutAnchorFrame& frame);
bool layout_anchor_center_position(LayoutContext* lycon, ViewBlock* box,
                                   LayoutAxis axis, float min, float max,
                                   float margin_box_width, float* result);
RdtLogicalPoint layout_anchor_scroll_adjustment(ViewBlock* box);
