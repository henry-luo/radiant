#include "test_native_provider.hpp"
#include "../radiant/event_host.hpp"

static UiContext* observed_ui;
static DomDocument* observed_document;
static RdtEvent* observed_event;

static void dispatch(UiContext* ui, DomDocument* document, RdtEvent* event) {
    observed_ui = ui;
    observed_document = document;
    observed_event = event;
}

static bool focusable(DomNode*, bool programmatic) { return programmatic; }
static void sync_pseudo_state(DomNode*, uint32_t, bool) {}
static void sync_focus_style(DomNode*, uint32_t) {}
static bool composition(UiContext*, RdtEvent*) { return true; }
static bool focused_caret_rect(UiContext*, float*, float*, float*, float*) { return true; }
static void initialize_behavior(DomDocument*, BehaviorInitPhaseTiming*) {}
static void autofocus(UiContext*, DomDocument*) {}
static void window_event(UiContext*, DomDocument*, const char*) {}
static void body_load(UiContext*, DomDocument*) {}
static bool content_visibility(UiContext*, DomElement*, bool) { return true; }
static void css_event(UiContext*, DomElement*, const char*, const char*, const char*, double) {}
static void svg_time_event(UiContext*, DomElement*, const char*, double, double) {}
static void scene_animation(UiContext*, DomElement*, const char*, uint64_t, const AnimationTimelineEvent&, uint64_t, uint64_t) {}
static bool scene_state(UiContext*, DomElement*, const char*, const char*, const char*, double) { return true; }
static bool allow_frame_reflow() { return true; }
static bool frame_tick(DomDocument*, double) { return true; }
static bool pending_frames(DomDocument*) { return true; }
static bool focus_element(DomDocument*, DomNode*) { return true; }
static bool collect_dom(UiContext*, DomDocument*) { return true; }
static bool editing_animation_active(DocState*) { return true; }
static bool editing_animation_tick(UiContext*, double) { return true; }
static bool input_capabilities(UiContext*, const CssInputCapabilities*) { return true; }

TEST(RadiantEventHostTest, DeliversBorrowedEventsOnlyToRegisteredController) {
    int marker = 0;
    auto* ui = reinterpret_cast<UiContext*>(&marker);
    auto* document = reinterpret_cast<DomDocument*>(&marker);
    auto* event = reinterpret_cast<RdtEvent*>(&marker);
    EXPECT_FALSE(radiant_event_host_dispatch(ui, document, event));
    float x = 1, y = 1, width = 1, height = 1;
    EXPECT_FALSE(radiant_event_host_focused_caret_rect(ui, &x, &y, &width, &height));
    EXPECT_FLOAT_EQ(x + y + width + height, 0.0f);
    static const RadiantEventHostOps incomplete = {nullptr, focusable, sync_pseudo_state, sync_focus_style, composition, focused_caret_rect, initialize_behavior, autofocus, window_event, body_load, content_visibility, css_event, svg_time_event, scene_animation, scene_state, allow_frame_reflow, frame_tick, pending_frames, focus_element, collect_dom, editing_animation_active, editing_animation_tick, input_capabilities};
    static const RadiantEventHostOps host = {dispatch, focusable, sync_pseudo_state, sync_focus_style, composition, focused_caret_rect, initialize_behavior, autofocus, window_event, body_load, content_visibility, css_event, svg_time_event, scene_animation, scene_state, allow_frame_reflow, frame_tick, pending_frames, focus_element, collect_dom, editing_animation_active, editing_animation_tick, input_capabilities};
    static const RadiantEventHostOps conflict = {dispatch, focusable, sync_pseudo_state, sync_focus_style, composition, focused_caret_rect, initialize_behavior, autofocus, window_event, body_load, content_visibility, css_event, svg_time_event, scene_animation, scene_state, allow_frame_reflow, frame_tick, pending_frames, focus_element, collect_dom, editing_animation_active, editing_animation_tick, input_capabilities};
    ASSERT_TRUE(expect_native_provider_registration(radiant_event_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_FALSE(radiant_event_host_dispatch(ui, document, nullptr));
    ASSERT_TRUE(radiant_event_host_dispatch(ui, document, event));
    EXPECT_TRUE(radiant_event_host_composition(ui, event));
    EXPECT_TRUE(radiant_event_host_focused_caret_rect(ui, &x, &y, &width, &height));
    EXPECT_FALSE(radiant_event_host_focusable(nullptr, false));
    EXPECT_TRUE(radiant_event_host_focusable(nullptr, true));
    EXPECT_EQ(observed_ui, ui);
    EXPECT_EQ(observed_document, document);
    EXPECT_EQ(observed_event, event);
}
