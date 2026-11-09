#include "test_native_provider.hpp"
#include "radiant/document_clock.hpp"
#include <stddef.h>
#include <uv.h>

alignas(max_align_t) static unsigned char document_marker;
static DomDocument* const test_document = reinterpret_cast<DomDocument*>(&document_marker);

static bool read_test_clock(DomDocument* document, double* now_ms) {
    if (now_ms) *now_ms = document == test_document ? 125.5 : 37.0;
    return document == test_document;
}

// One test owns registration so absence and conflicting providers remain observable.
TEST(RadiantClockProviderTest, NativeFallbackAndHostPublication) {
    double before = (double)uv_hrtime() / 1e6;
    double now = -1;
    EXPECT_FALSE(radiant_document_clock(nullptr, &now));
    double after = (double)uv_hrtime() / 1e6;
    EXPECT_GE(now, before);
    EXPECT_LE(now, after);
    EXPECT_FALSE(radiant_document_clock(nullptr, nullptr));
    static const RadiantDocumentClockOps incomplete = {};
    static const RadiantDocumentClockOps host = {read_test_clock};
    static const RadiantDocumentClockOps conflict = {read_test_clock};
    ASSERT_TRUE(expect_native_provider_registration(radiant_document_clock_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_document_clock(test_document, &now));
    EXPECT_DOUBLE_EQ(now, 125.5);
    EXPECT_TRUE(radiant_document_clock(test_document, nullptr));
    EXPECT_FALSE(radiant_document_clock(nullptr, &now));
    EXPECT_DOUBLE_EQ(now, 37.0);
}
