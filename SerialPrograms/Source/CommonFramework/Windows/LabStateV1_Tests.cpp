/*  Lab State V1 - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <utility>
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabStateV1.h"
#include "LabStateV1_Tests.h"

namespace PokemonAutomation{
namespace{


struct LabStateTestInput{
    LabStateV1 state;
};

struct LabStateTestExpectation{
    std::string_view json;
};

struct LabStateTestCase{
    std::string name;
    LabStateTestInput input;
    LabStateTestExpectation expectation;
};

class Test_LabStateV1 : public UnitTest{
public:
    explicit Test_LabStateV1(LabStateTestCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateV1 - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const JsonValue actual = parse_json(lab_state_json_text(m_case.input.state));
        const JsonValue expected = parse_json(std::string(m_case.expectation.json));
        if (actual != expected){
            return std::format(
                "Expected {}, got {}", m_case.expectation.json, lab_state_json_text(m_case.input.state)
            );
        }
        return true;
    }

private:
    LabStateTestCase m_case;
};

const LabStateTestCase TEST_CASES[] = {
    {
        .name = "observed stopped program",
        .input = {
            .state = {
                .written_at = "2026-11-14T06:10:12.124Z",
                .instance_id = "app-1",
                .process_id = 4411,
                .panel_id = "PokemonFRLG:AutoStory",
                .panel_kind = LabStateV1::PanelKind::PROGRAM,
                .program_state = LabStateV1::ProgramState::STOPPED,
                .run_id = std::nullopt,
                .transition_action = "stop",
                .transition_source = LabStateV1::ActionSource::LAB,
                .transition_request_id = "request-1",
                .transition_at = "2026-11-14T06:10:10.002Z",
                .controller_state = LabStateV1::ControllerState::READY,
                .video_state = LabStateV1::VideoState::LIVE,
                .video_frame_seq = 918274,
                .video_captured_at = "2026-11-14T06:10:12.091Z",
                .video_signal = LabStateV1::SignalState::UNKNOWN,
            },
        },
        .expectation = {
            .json = R"json({
                "schema": 1,
                "written_at": "2026-11-14T06:10:12.124Z",
                "instance": {"id": "app-1", "pid": 4411},
                "panel": {"id": "PokemonFRLG:AutoStory", "kind": "program"},
                "program": {
                    "state": "stopped",
                    "run_id": null,
                    "last_transition": {
                        "action": "stop",
                        "source": "lab",
                        "request_id": "request-1",
                        "at": "2026-11-14T06:10:10.002Z"
                    }
                },
                "controller": {"state": "ready"},
                "video": {
                    "state": "live",
                    "frame_seq": 918274,
                    "captured_at": "2026-11-14T06:10:12.091Z",
                    "signal": "unknown"
                },
                "observation": {
                    "game": {"value": "unknown", "observed_at": null},
                    "screen": {"value": "unknown", "observed_at": null}
                }
            })json",
        },
    },
    {
        .name = "unobserved defaults",
        .input = {.state = {}},
        .expectation = {
            .json = R"json({
                "schema": 1,
                "written_at": "",
                "instance": {"id": "", "pid": 0},
                "panel": {"id": "", "kind": "none"},
                "program": {"state": "no_program", "run_id": null, "last_transition": null},
                "controller": {"state": "not_applicable"},
                "video": {"state": "missing", "frame_seq": null, "captured_at": null, "signal": "unknown"},
                "observation": {
                    "game": {"value": "unknown", "observed_at": null},
                    "screen": {"value": "unknown", "observed_at": null}
                }
            })json",
        },
    },
};


} // namespace

void add_tests_LabStateV1(UnitTestDatabase& database){
    for (const LabStateTestCase& test_case : TEST_CASES){
        database.add<Test_LabStateV1>(test_case);
    }
}


} // namespace PokemonAutomation
