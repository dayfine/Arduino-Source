/*  Command File Recorder - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "NintendoSwitch_CommandFileRecorder.h"
#include "NintendoSwitch_CommandFileRecorder_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{

namespace{

//  2026-11-08T21:04:10.000Z.
constexpr int64_t START_MS = 1794171850000;

struct StepCase{
    std::string name;
    struct Input{
        size_t line = 0;
        std::vector<std::string> words = {};
        int64_t duration_ms = 0;
        std::optional<std::string> before = {};     //  The detectors' JSON object, if a game registered any.
        std::optional<std::string> after = {};
        std::optional<std::string> error = {};
        std::string screenshot = {};
    };
    Input input;
    struct Expectation{
        std::string json;       //  The steps file's line, without the newline.
    };
    Expectation expectation;
};

const StepCase STEP_CASES[] = {
    {
        .name = "ok without game state",
        .input = {
            .line = 2, .words = {"press", "A"}, .duration_ms = 322,
            .screenshot = "/outbox/probe_step_L2.jpg",
        },
        .expectation = {.json =
            R"json({"args":["A"],"command":"press","duration_ms":322,"end":"2026-11-08T21:04:10.322Z",)json"
            R"json("line":2,"ok":true,"screenshot":"/outbox/probe_step_L2.jpg","start":"2026-11-08T21:04:10.000Z"})json"
        },
    },
    {
        .name = "failed with game state",
        .input = {
            .line = 7, .words = {"game_move", "90", "1500"}, .duration_ms = 1640,
            .before = R"json({"game_dialog":false,"game_facing":87})json",
            .after = R"json({"game_dialog":true,"game_facing":null})json",
            .error = "game_move: a day/night change cut the walk short.",
            .screenshot = "/outbox/probe_step_L7.jpg",
        },
        .expectation = {.json =
            R"json({"after":{"game_dialog":true,"game_facing":null},"args":["90","1500"],)json"
            R"json("before":{"game_dialog":false,"game_facing":87},"command":"game_move","duration_ms":1640,)json"
            R"json("end":"2026-11-08T21:04:11.640Z","error":"game_move: a day/night change cut the walk short.",)json"
            R"json("line":7,"ok":false,"screenshot":"/outbox/probe_step_L7.jpg","start":"2026-11-08T21:04:10.000Z"})json"
        },
    },
    {
        .name = "no arguments",
        .input = {.line = 1, .words = {"game_battle"}, .duration_ms = 0, .screenshot = "(no video frame)"},
        .expectation = {.json =
            R"json({"args":[],"command":"game_battle","duration_ms":0,"end":"2026-11-08T21:04:10.000Z",)json"
            R"json("line":1,"ok":true,"screenshot":"(no video frame)","start":"2026-11-08T21:04:10.000Z"})json"
        },
    },
};

//  The object `text` holds, or an empty one for none.
JsonObject json_object(const std::optional<std::string>& text){
    if (!text){
        return JsonObject();
    }
    return parse_json(*text).to_object_throw().clone();
}

UnitTestResult check_step_case(const StepCase& test){
    const StepCase::Input& input = test.input;
    const WallClock start{std::chrono::milliseconds(START_MS)};
    CommandFileStep step{
        .line = input.line,
        .words = input.words,
        .start = start,
        .end = start + std::chrono::milliseconds(input.duration_ms),
        .before = json_object(input.before),
        .after = json_object(input.after),
        .error = input.error,
        .screenshot = input.screenshot,
    };
    const std::string read = command_file_step_json(step);
    if (read != test.expectation.json){
        return std::format("Expected the line\n{}\nread\n{}", test.expectation.json, read);
    }
    return true;
}

class StepJsonTest : public UnitTest{
public:
    explicit StepJsonTest(const StepCase& test)
        : UnitTest("NintendoSwitch::CommandFileRecorder - " + test.name)
        , m_test(test)
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_step_case(m_test);
    }

private:
    const StepCase& m_test;
};

} // namespace


void add_tests_CommandFileRecorder(UnitTestDatabase& database){
    for (const StepCase& test : STEP_CASES){
        database.add<StepJsonTest>(test);
    }
}



} // namespace NintendoSwitch
} // namespace PokemonAutomation
