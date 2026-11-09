#include <gtest/gtest.h>
#include "../radiant/rdt_speech.h"
#include "../radiant/rdt_video.h"
#include <string.h>

TEST(RadiantNativeMedia, VideoCapabilitiesUseThePlatformProvider) {
    const char* mp4 = "video/mp4";
#ifdef __APPLE__
    EXPECT_STREQ("maybe", rdt_video_can_play_type(mp4, strlen(mp4)));
    const char* h264 = "video/mp4; codecs=\"avc1.42E01E\"";
    EXPECT_STREQ("probably", rdt_video_can_play_type(h264, strlen(h264)));
#else
    EXPECT_STREQ("", rdt_video_can_play_type(mp4, strlen(mp4)));
#endif
}

TEST(RadiantNativeMedia, SpeechProviderReportsAnUnavailableVoice) {
    RdtSpeechRequest request = {};
    request.text = "test";
    request.length = 4;
    request.voice_id = "invalid.lambda.test.voice";
    RdtSpeechJob* job = rdt_speech_render_begin(&request);
#ifdef __APPLE__
    ASSERT_NE(job, nullptr);
    EXPECT_EQ(rdt_speech_job_state(job), RDT_SPEECH_FAILED);
    const char* error = rdt_speech_job_error(job);
    EXPECT_NE(error, nullptr);
    if (error) EXPECT_NE(strstr(error, "no voice"), nullptr);
    rdt_speech_job_destroy(job);
#else
    EXPECT_EQ(job, nullptr);
#endif
}
