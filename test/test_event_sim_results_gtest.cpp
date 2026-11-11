#include <gtest/gtest.h>
#include "radiant/event_sim.hpp"
#include "lib/file.h"
#include "lib/log.h"
#include "lib/mem.h"
#include "lib/shell.h"
#include <string.h>

static const char* executable;

TEST(EventSimResults, HumanAndMachineResultsRemainAvailableWithLoggingDisabled) {
    ASSERT_EQ(file_ensure_dir("temp"), 0);
    for (const char* mode : {"pass", "fail", "empty"}) {
        SCOPED_TRACE(mode);
        const char* args[] = {executable, "--event-result-worker", mode, nullptr};
        ShellOptions options = {};
        options.merge_stderr = true;
        ShellResult result = shell_exec(executable, args, &options);
        EXPECT_EQ(result.exit_code, 0);
        ASSERT_NE(result.stdout_buf, nullptr);
        EXPECT_NE(strstr(result.stdout_buf, "EVENT SIMULATION RESULTS"), nullptr);
        EXPECT_NE(strstr(result.stdout_buf, "Test: result-contract"), nullptr);
        EXPECT_NE(strstr(result.stdout_buf, "Events executed: 3"), nullptr);
        EXPECT_NE(strstr(result.stdout_buf, strcmp(mode, "pass") == 0
            ? "Assertions: 2 passed, 0 failed" : strcmp(mode, "fail") == 0
            ? "Assertions: 1 passed, 1 failed" : "Assertions: 0 passed, 0 failed"), nullptr);
        if (strcmp(mode, "empty") == 0) EXPECT_EQ(strstr(result.stdout_buf, "Result:"), nullptr);
        else EXPECT_NE(strstr(result.stdout_buf, strcmp(mode, "pass") == 0 ? "Result: PASS" : "Result: FAIL"), nullptr);
        shell_result_free(&result);
        char* machine = read_text_file("temp/event-sim-result-contract.json");
        ASSERT_NE(machine, nullptr);
        EXPECT_NE(strstr(machine, strcmp(mode, "pass") == 0 ? "\"passed\":2,\"failed\":0"
            : strcmp(mode, "fail") == 0 ? "\"passed\":1,\"failed\":1" : "\"passed\":0,\"failed\":0"), nullptr);
        EXPECT_NE(strstr(machine, strcmp(mode, "fail") == 0 ? "\"result\":\"FAIL\"" : "\"result\":\"PASS\""), nullptr);
        mem_free(machine);
    }
}

int main(int argc, char** argv) {
    if (argc == 3 && strcmp(argv[1], "--event-result-worker") == 0) {
        log_set_transient_min_level(LOG_LEVEL_FATAL);
        EventSimContext context = {};
        char name[] = "result-contract";
        char path[] = "temp/event-sim-result-contract.json";
        context.test_name = name;
        context.result_path = path;
        context.current_index = 3;
        context.pass_count = strcmp(argv[2], "pass") == 0 ? 2 : strcmp(argv[2], "fail") == 0 ? 1 : 0;
        context.fail_count = strcmp(argv[2], "fail") == 0 ? 1 : 0;
        event_sim_print_results(&context);
        return 0;
    }
    executable = argv[0];
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
