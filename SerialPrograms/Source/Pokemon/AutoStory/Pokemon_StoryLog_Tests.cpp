/*  Story Log Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryLog.h"
#include "Pokemon_StoryLog_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

// One formatter call and the line it must make.
struct StoryLogTestCase{
    //  A formatter's line, made when the table is built.
    struct Input{
        std::string line;
    };

    //  The exact line: the lab watcher and the engine tests read these.
    struct Expectation{
        std::string line;
    };

    std::string name;
    Input input;
    Expectation expectation;
};

class Test_StoryLog : public UnitTest{
public:
    Test_StoryLog(StoryLogTestCase test_case)
        : UnitTest(std::format("Pokemon::StoryLog - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (m_case.input.line != m_case.expectation.line){
            return std::format("Expected \"{}\", got \"{}\"", m_case.expectation.line, m_case.input.line);
        }
        return true;
    }

private:
    const StoryLogTestCase m_case;
};

//  SV's display text for a checkpoint, which already says "Checkpoint::".
constexpr std::string_view SV_TEXT = "Checkpoint:: 004 - 02. Start point: X";

const StoryLogTestCase STORY_LOG_CASES[] = {
    {
        .name = "checkpoint start",
        .input = {.line = checkpoint_start_line("00.04")},
        .expectation = {.line = "[Checkpoint] 00.04: start"},
    },
    {
        .name = "checkpoint start with its events",
        .input = {.line = checkpoint_start_line("00.04", 3)},
        .expectation = {.line = "[Checkpoint] 00.04: start, 3 events"},
    },
    {
        .name = "attempt",
        .input = {.line = checkpoint_attempt_line("00.04", 2)},
        .expectation = {.line = "[Checkpoint] 00.04: attempt 2"},
    },
    {
        .name = "attempt out of a limit",
        .input = {.line = checkpoint_attempt_line("00.04", 2, 3)},
        .expectation = {.line = "[Checkpoint] 00.04: attempt 2/3"},
    },
    {
        .name = "one-shot pass",
        .input = {.line = checkpoint_pass_line("00.03", 1)},
        .expectation = {.line = "[Checkpoint] 00.03: passed on attempt 1 (one-shot)"},
    },
    {
        .name = "pass after one reset",
        .input = {.line = checkpoint_pass_line("00.04", 2)},
        .expectation = {.line = "[Checkpoint] 00.04: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "pass after resets",
        .input = {.line = checkpoint_pass_line("00.04", 3)},
        .expectation = {.line = "[Checkpoint] 00.04: passed on attempt 3 (2 resets)"},
    },
    {
        .name = "reset",
        .input = {.line = checkpoint_reset_line("00.04", 2)},
        .expectation = {.line = "[Checkpoint] 00.04: failed, resetting for attempt 2"},
    },
    {
        .name = "reset out of a limit",
        .input = {.line = checkpoint_reset_line("00.04", 2, 3)},
        .expectation = {.line = "[Checkpoint] 00.04: failed, resetting for attempt 2/3"},
    },
    {
        .name = "start with display text",
        .input = {.line = checkpoint_start_line("004", std::nullopt, SV_TEXT)},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: start"},
    },
    {
        .name = "attempt with display text",
        .input = {.line = checkpoint_attempt_line("004", 2, 100, SV_TEXT)},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: attempt 2/100"},
    },
    {
        .name = "pass with display text",
        .input = {.line = checkpoint_pass_line("004", 2, SV_TEXT)},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: passed on attempt 2 (1 reset)"},
    },
    {
        .name = "reset with display text",
        .input = {.line = checkpoint_reset_line("004", 2, std::nullopt, SV_TEXT)},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: failed, resetting for attempt 2"},
    },
    {
        .name = "short checkpoint start",
        .input = {.line = story_short_line("[Checkpoint] 12.03: start, 18 events")},
        .expectation = {.line = "12.03: start, 18 events"},
    },
    {
        .name = "short line with another tag",
        .input = {.line = story_short_line("[Tag] 12.03: some text")},
        .expectation = {.line = "12.03: some text"},
    },
    {
        .name = "short line without a tag",
        .input = {.line = story_short_line("Checkpoint:: 004 - 02. Start point: X: start")},
        .expectation = {.line = "Checkpoint:: 004 - 02. Start point: X: start"},
    },
    {
        .name = "short line with an unclosed tag",
        .input = {.line = story_short_line("[Tag 12.03")},
        .expectation = {.line = "[Tag 12.03"},
    },
    {
        .name = "short empty line",
        .input = {.line = story_short_line("")},
        .expectation = {.line = ""},
    },
};


} // namespace


void add_tests_StoryLog(UnitTestDatabase& database){
    for (const StoryLogTestCase& test_case : STORY_LOG_CASES){
        database.add<Test_StoryLog>(test_case);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
