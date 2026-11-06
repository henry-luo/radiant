#include "test_native_provider.hpp"
#include "../radiant/event_sim_script_host.hpp"

static bool evaluate(UiContext*, const char* source, bool assertion) {
    return source && assertion;
}
static bool collect_gc(UiContext*, double count) { return count == 2; }
static bool animation_frame(UiContext*, double time, int* callbacks) {
    *callbacks = 3;
    return time == 10;
}
static bool measure_frames(UiContext*, const char*, const char*, size_t frames,
                           double* durations, double* sampling) {
    if (frames != 1) return false;
    durations[0] = 4;
    sampling[0] = 2;
    return true;
}

static const char* observed_dialog;
static DomDocument* observed_document;
static Pool* observed_pool;
static SelectorMatcher* provided_matcher;
static DomElement* provided_element;
static float observed_x, observed_y;
static double clock_ms(bool relative) { return relative ? 7.0 : 11.0; }
static void dialog_response(const char* value) { observed_dialog = value; }
static SelectorMatcher* selector_matcher(DomDocument* document, Pool* pool) {
    observed_document = document;
    observed_pool = pool;
    return provided_matcher;
}
static DomElement* element_from_point(DomDocument* document, float x, float y) {
    observed_document = document;
    observed_x = x;
    observed_y = y;
    return provided_element;
}

TEST(RadiantEventSimScriptHostTest, MissingHostAndRegisteredOperations) {
    int callbacks = 0;
    double duration = 0, sample = 0;
    EXPECT_FALSE(radiant_event_sim_evaluate(nullptr, "true", true));
    EXPECT_FALSE(radiant_event_sim_collect_gc(nullptr, 2));
    EXPECT_FALSE(radiant_event_sim_animation_frame(nullptr, 10, &callbacks));
    EXPECT_FALSE(radiant_event_sim_measure_frames(nullptr, "f", "s", 1, &duration, &sample));
    EXPECT_GT(radiant_event_sim_clock_ms(false), 0.0);
    EXPECT_GT(radiant_event_sim_clock_ms(true), 0.0);
    radiant_event_sim_dialog_response("ignored");
    EXPECT_EQ(observed_dialog, nullptr);
    SelectorMatcher* matcher = nullptr;
    EXPECT_FALSE(radiant_event_sim_script_selector(nullptr, nullptr, &matcher));
    DomElement* element = nullptr;
    EXPECT_FALSE(radiant_event_sim_script_hit_test(nullptr, 1, 2, &element));
    static const RadiantEventSimScriptHostOps incomplete = {evaluate, collect_gc, animation_frame, measure_frames, clock_ms, dialog_response, selector_matcher, nullptr};
    static const RadiantEventSimScriptHostOps host = {evaluate, collect_gc, animation_frame, measure_frames, clock_ms, dialog_response, selector_matcher, element_from_point};
    static const RadiantEventSimScriptHostOps conflict = {evaluate, collect_gc, animation_frame, measure_frames, clock_ms, dialog_response, selector_matcher, element_from_point};
    ASSERT_TRUE(expect_native_provider_registration(radiant_event_sim_script_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_event_sim_evaluate(nullptr, "true", true));
    EXPECT_TRUE(radiant_event_sim_collect_gc(nullptr, 2));
    EXPECT_TRUE(radiant_event_sim_animation_frame(nullptr, 10, &callbacks));
    EXPECT_EQ(callbacks, 3);
    EXPECT_TRUE(radiant_event_sim_measure_frames(nullptr, "f", "s", 1, &duration, &sample));
    EXPECT_DOUBLE_EQ(duration, 4);
    EXPECT_DOUBLE_EQ(sample, 2);
    EXPECT_DOUBLE_EQ(radiant_event_sim_clock_ms(false), 11.0);
    EXPECT_DOUBLE_EQ(radiant_event_sim_clock_ms(true), 7.0);
    radiant_event_sim_dialog_response("answer");
    EXPECT_STREQ(observed_dialog, "answer");
    int marker = 0;
    auto* document = reinterpret_cast<DomDocument*>(&marker);
    auto* pool = reinterpret_cast<Pool*>(&marker);
    provided_matcher = reinterpret_cast<SelectorMatcher*>(&marker);
    EXPECT_TRUE(radiant_event_sim_script_selector(document, pool, &matcher));
    EXPECT_EQ(observed_document, document);
    EXPECT_EQ(observed_pool, pool);
    EXPECT_EQ(matcher, provided_matcher);
    provided_matcher = nullptr;
    EXPECT_TRUE(radiant_event_sim_script_selector(document, pool, &matcher));
    EXPECT_EQ(matcher, nullptr);
    provided_element = reinterpret_cast<DomElement*>(&marker);
    EXPECT_TRUE(radiant_event_sim_script_hit_test(document, 1.5f, 2.5f, &element));
    EXPECT_EQ(observed_document, document);
    EXPECT_FLOAT_EQ(observed_x, 1.5f);
    EXPECT_FLOAT_EQ(observed_y, 2.5f);
    EXPECT_EQ(element, provided_element);
    provided_element = nullptr;
    EXPECT_TRUE(radiant_event_sim_script_hit_test(document, 1, 2, &element));
    EXPECT_EQ(element, nullptr);
}
