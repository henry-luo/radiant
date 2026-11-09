#include "test_native_provider.hpp"
#include "radiant/speech_event_host.hpp"

static DomElement* observed_target;
static uint64_t observed_token;
static const char* observed_type;
static int64_t observed_char_index;

static bool publish_speech_event(DomElement* target, uint64_t token, const char* type,
                                 const SpeechEventFields* fields) {
    observed_target = target;
    observed_token = token;
    observed_type = type;
    observed_char_index = fields->char_index;
    return true;
}

TEST(RadiantSpeechEventHostTest, MissingHostAndRegisteredPublication) {
    int marker = 0;
    auto* target = reinterpret_cast<DomElement*>(&marker);
    SpeechEventFields fields = {};
    fields.char_index = 42;
    EXPECT_FALSE(radiant_speech_event_publish(target, 7, "speechboundary", &fields));
    static const RadiantSpeechEventHostOps incomplete = {nullptr};
    static const RadiantSpeechEventHostOps host = {publish_speech_event};
    static const RadiantSpeechEventHostOps conflict = {publish_speech_event};
    ASSERT_TRUE(expect_native_provider_registration(radiant_speech_event_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_speech_event_publish(target, 7, "speechboundary", &fields));
    EXPECT_EQ(observed_target, target);
    EXPECT_EQ(observed_token, 7u);
    EXPECT_STREQ(observed_type, "speechboundary");
    EXPECT_EQ(observed_char_index, 42);
}
