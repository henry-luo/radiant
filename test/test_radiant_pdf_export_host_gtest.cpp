#include "test_native_provider.hpp"
#include "../radiant/pdf_export_host.hpp"

static RenderExportSession* observed_session;
static const char* observed_filename;
static const ViewPageSelection* observed_selection;

static bool write_pdf(RenderExportSession* session, const char* filename,
                      const ViewPageSelection* selection) {
    observed_session = session;
    observed_filename = filename;
    observed_selection = selection;
    return true;
}

TEST(RadiantPdfExportHostTest, MissingHostAndRegisteredWriter) {
    int marker = 0;
    auto* session = reinterpret_cast<RenderExportSession*>(&marker);
    auto* selection = reinterpret_cast<ViewPageSelection*>(&marker);
    EXPECT_FALSE(radiant_pdf_export_write(session, "result.pdf", selection));
    static const RadiantPdfExportHostOps incomplete = {nullptr};
    static const RadiantPdfExportHostOps host = {write_pdf};
    static const RadiantPdfExportHostOps conflict = {write_pdf};
    ASSERT_TRUE(expect_native_provider_registration(radiant_pdf_export_host_register,
                                                     &incomplete, &host, &conflict));
    EXPECT_TRUE(radiant_pdf_export_write(session, "result.pdf", selection));
    EXPECT_EQ(observed_session, session);
    EXPECT_STREQ(observed_filename, "result.pdf");
    EXPECT_EQ(observed_selection, selection);
}
