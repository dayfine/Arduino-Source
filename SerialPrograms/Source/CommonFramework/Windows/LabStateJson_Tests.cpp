/*  Lab State JSON - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "Lab/state.json" against the lab server's version-1 goldens, the video
 *  state, and where each start and stop is said to come from.
 *
 *  The goldens are copies of lab/server/contracts/v1/app_state/{stopped_ui,
 *  running_lab,no_program}.json on the fork's agents branch (fork PR #478 at
 *  ff209b333). Change them only together with the contract.
 *
 */

#include <format>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabStateJson.h"
#include "LabStateJson_Tests.h"

namespace PokemonAutomation{

namespace{

using namespace std::chrono_literals;


WallClock utc(int year, unsigned month, unsigned day, Milliseconds time_of_day){
    const std::chrono::sys_days date = std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day};
    return WallClock(std::chrono::duration_cast<WallDuration>(date.time_since_epoch() + time_of_day));
}
Milliseconds hms(int hours, int minutes, int seconds, int millis){
    return std::chrono::hours(hours) + std::chrono::minutes(minutes) + Seconds(seconds) + Milliseconds(millis);
}


constexpr std::string_view GOLDEN_STOPPED_UI = R"({
  "schema": 1,
  "written_at": "2026-11-14T06:10:12.124Z",
  "instance": {
    "id": "d8ea6a9d-1eab-4a40-b355-d88072b289a1",
    "pid": 4411,
    "profile": "lab-B"
  },
  "panel": {
    "id": "PokemonFRLG:AutoStory",
    "kind": "program"
  },
  "program": {
    "state": "stopped",
    "run_id": null,
    "last_transition": {
      "action": "stop",
      "source": "ui",
      "request_id": null,
      "at": "2026-11-14T06:10:10.002Z"
    }
  },
  "controller": {
    "state": "ready"
  },
  "video": {
    "state": "live",
    "frame_seq": 918274,
    "captured_at": "2026-11-14T06:10:12.091Z",
    "signal": "unknown"
  },
  "observation": {
    "game": {
      "value": "unknown",
      "observed_at": null
    },
    "screen": {
      "value": "unknown",
      "observed_at": null
    }
  },
  "channel": {
    "last_request": "fs012327",
    "last_error": null,
    "owner_claimed": false
  }
}
)";

constexpr std::string_view GOLDEN_RUNNING_LAB = R"({
  "schema": 1,
  "written_at": "2026-11-14T07:02:41.500Z",
  "instance": {
    "id": "d8ea6a9d-1eab-4a40-b355-d88072b289a1",
    "pid": 4411,
    "profile": "lab-B"
  },
  "panel": {
    "id": "PokemonBDSP:AutoStory",
    "kind": "program"
  },
  "program": {
    "state": "running",
    "run_id": null,
    "last_transition": {
      "action": "start",
      "source": "lab",
      "request_id": "5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10",
      "at": "2026-11-14T07:01:03.250Z"
    },
    "paused": "01.04 leg 3"
  },
  "controller": {
    "state": "ready"
  },
  "video": {
    "state": "live",
    "frame_seq": 921930,
    "captured_at": "2026-11-14T07:02:41.466Z",
    "signal": "unknown"
  },
  "observation": {
    "game": {
      "value": "PokemonBDSP",
      "observed_at": "2026-11-14T07:01:02.900Z",
      "detector": "bdsp_overworld",
      "confidence": 0.97
    },
    "screen": {
      "value": "unknown",
      "observed_at": null
    }
  },
  "channel": {
    "last_request": "r42",
    "last_error": null,
    "owner_claimed": true
  }
}
)";

constexpr std::string_view GOLDEN_NO_PROGRAM = R"({
  "schema": 1,
  "written_at": "2026-11-14T05:00:00.000Z",
  "instance": {
    "id": "0c7b3f52-91d4-4e8a-a3f6-5b2e7d10c9a4",
    "pid": 3902
  },
  "panel": {
    "id": null,
    "kind": "other"
  },
  "program": {
    "state": "no_program",
    "run_id": null,
    "last_transition": null
  },
  "controller": {
    "state": "no_console"
  },
  "video": {
    "state": "missing",
    "frame_seq": null,
    "captured_at": null,
    "signal": "unknown"
  },
  "observation": {
    "game": {
      "value": "unknown",
      "observed_at": null
    },
    "screen": {
      "value": "unknown",
      "observed_at": null
    }
  },
  "channel": {
    "last_request": "r1",
    "last_error": "The selected panel is not a program.",
    "owner_claimed": false
  }
}
)";


LabAppState stopped_ui(){
    LabAppState state;
    state.legacy = {
        .seq = "fs012327", .state = "STOPPED", .program = "PokemonFRLG:AutoStory",
        .controller = "ready", .owner = "", .error = "",
    };
    state.written_at = utc(2026, 11, 14, hms(6, 10, 12, 124));
    state.instance_id = "d8ea6a9d-1eab-4a40-b355-d88072b289a1";
    state.pid = 4411;
    state.profile = "lab-B";
    state.last_transition = LabTransition{
        .action = LabTransitionAction::STOP, .source = LabTransitionSource::UI,
        .request_id = std::nullopt, .at = utc(2026, 11, 14, hms(6, 10, 10, 2)),
    };
    state.video = LabVideoState::LIVE;
    state.frame = LabFrameToken{.seq = 918274, .captured_at = utc(2026, 11, 14, hms(6, 10, 12, 91))};
    return state;
}

LabAppState running_lab(){
    LabAppState state;
    state.legacy = {
        .seq = "r42", .state = "RUNNING", .program = "PokemonBDSP:AutoStory",
        .controller = "ready", .owner = "pa-lab-daemon", .error = "",
    };
    state.written_at = utc(2026, 11, 14, hms(7, 2, 41, 500));
    state.instance_id = "d8ea6a9d-1eab-4a40-b355-d88072b289a1";
    state.pid = 4411;
    state.profile = "lab-B";
    state.last_transition = LabTransition{
        .action = LabTransitionAction::START, .source = LabTransitionSource::LAB,
        .request_id = "5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10", .at = utc(2026, 11, 14, hms(7, 1, 3, 250)),
    };
    state.paused = "01.04 leg 3";
    state.video = LabVideoState::LIVE;
    state.frame = LabFrameToken{.seq = 921930, .captured_at = utc(2026, 11, 14, hms(7, 2, 41, 466))};
    state.game = LabObservation{
        .value = "PokemonBDSP", .observed_at = utc(2026, 11, 14, hms(7, 1, 2, 900)),
        .detector = "bdsp_overworld", .confidence = 0.97,
    };
    return state;
}

LabAppState no_program(){
    LabAppState state;
    state.legacy = {
        .seq = "r1", .state = "NO_PROGRAM", .program = "",
        .controller = "", .owner = "", .error = "The selected panel is not a program.",
    };
    state.written_at = utc(2026, 11, 14, hms(5, 0, 0, 0));
    state.instance_id = "0c7b3f52-91d4-4e8a-a3f6-5b2e7d10c9a4";
    state.pid = 3902;
    return state;
}


struct GoldenInput{
    std::function<LabAppState()> state;
};
struct GoldenExpectation{
    std::string_view json;
};
struct GoldenCase{
    std::string name;
    GoldenInput input;
    GoldenExpectation expectation;
};
UnitTestResult check(const GoldenCase& c){
    const std::string text = lab_state_json_text(c.input.state());
    if (text != c.expectation.json){
        return std::format("expected\n{}\ngot\n{}", c.expectation.json, text);
    }
    return true;
}
class Test_LabStateGolden : public UnitTest{
public:
    explicit Test_LabStateGolden(GoldenCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateJson - golden - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    GoldenCase m_case;
};
const GoldenCase GOLDEN_CASES[] = {
    {.name = "stopped by the UI", .input = {.state = stopped_ui}, .expectation = {.json = GOLDEN_STOPPED_UI}},
    {.name = "started by the lab", .input = {.state = running_lab}, .expectation = {.json = GOLDEN_RUNNING_LAB}},
    {.name = "no program", .input = {.state = no_program}, .expectation = {.json = GOLDEN_NO_PROGRAM}},
};


//  One legacy field changed in the stopped_ui() golden state.
struct FieldInput{
    std::string state = "STOPPED";
    std::string controller = "ready";
    std::string error = {};
};
struct FieldExpectation{
    std::vector<std::string> lines;     //  Text the JSON must contain.
};
struct FieldCase{
    std::string name;
    FieldInput input;
    FieldExpectation expectation;
};
UnitTestResult check(const FieldCase& c){
    LabAppState state = stopped_ui();
    state.legacy.state = c.input.state;
    state.legacy.controller = c.input.controller;
    state.legacy.error = c.input.error;
    const std::string text = lab_state_json_text(state);
    for (const std::string& line : c.expectation.lines){
        if (text.find(line) == std::string::npos){
            return std::format("expected <{}> in\n{}", line, text);
        }
    }
    return true;
}
class Test_LabStateField : public UnitTest{
public:
    explicit Test_LabStateField(FieldCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateJson - field - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    FieldCase m_case;
};
const FieldCase FIELD_CASES[] = {
    {
        .name = "quote and backslash",
        .input = {.error = "a \"b\" \\c"},
        .expectation = {.lines = {R"("last_error": "a \"b\" \\c",)"}},
    },
    {
        .name = "control characters",
        .input = {.error = "a\nb\tc\x01"},
        .expectation = {.lines = {R"("last_error": "a\nb\tc\u0001",)"}},
    },
    {
        .name = "UTF-8 stays as is",
        .input = {.error = "Pok\xC3\xA9mon"},
        .expectation = {.lines = {"\"last_error\": \"Pok\xC3\xA9mon\","}},
    },
    {
        .name = "program without a controller",
        .input = {.state = "NOT_READY", .controller = "none"},
        .expectation = {.lines = {
            "\"state\": \"not_ready\"",
            "\"id\": \"PokemonFRLG:AutoStory\",\n    \"kind\": \"program\"",
            "\"controller\": {\n    \"state\": \"not_connected\"",
        }},
    },
    {
        //  The legacy file names any selected panel; state.json only a program.
        .name = "settings panel",
        .input = {.state = "NO_PROGRAM", .controller = ""},
        .expectation = {.lines = {
            "\"state\": \"no_program\"",
            "\"id\": null,\n    \"kind\": \"other\"",
            "\"controller\": {\n    \"state\": \"no_console\"",
        }},
    },
};


struct VideoInput{
    bool has_video;
    std::optional<Milliseconds> frame_age;  //  Of the newest frame; none before the first.
};
struct VideoExpectation{
    LabVideoState state;
};
struct VideoCase{
    std::string name;
    VideoInput input;
    VideoExpectation expectation;
};
UnitTestResult check(const VideoCase& c){
    const WallClock now = utc(2026, 11, 14, hms(6, 10, 12, 0));
    std::optional<LabFrameToken> frame;
    if (c.input.frame_age){
        frame = LabFrameToken{.seq = 7, .captured_at = now - *c.input.frame_age};
    }
    if (lab_video_state(c.input.has_video, frame, now, 2000ms) != c.expectation.state){
        return "wrong video state";
    }
    return true;
}
class Test_LabVideoState : public UnitTest{
public:
    explicit Test_LabVideoState(VideoCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateJson - video - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    VideoCase m_case;
};
const VideoCase VIDEO_CASES[] = {
    {.name = "fresh frame", .input = {.has_video = true, .frame_age = 500ms}, .expectation = {.state = LabVideoState::LIVE}},
    {.name = "old frame", .input = {.has_video = true, .frame_age = 5000ms}, .expectation = {.state = LabVideoState::STALE}},
    {.name = "no frame yet", .input = {.has_video = true, .frame_age = std::nullopt}, .expectation = {.state = LabVideoState::MISSING}},
    {.name = "panel without video", .input = {.has_video = false, .frame_age = 500ms}, .expectation = {.state = LabVideoState::MISSING}},
};


//  A state change, or a call the lab channel makes.
enum class Step{
    EXPECT_LAB_START,
    EXPECT_LAB_STOP,
    CANCEL_LAB,
    RUNNING,
    STOPPING,
    STOPPED,
    ERROR_REPORTED,
};
struct TransitionInput{
    std::vector<Step> steps;
};
struct TransitionExpectation{
    std::optional<LabTransitionAction> action;  //  None: no transition yet.
    LabTransitionSource source = LabTransitionSource::UI;
    bool has_request_id = false;
};
struct TransitionCase{
    std::string name;
    TransitionInput input;
    TransitionExpectation expectation;
};
void apply(LabTransitionTracker& tracker, Step step, WallClock now){
    switch (step){
    case Step::EXPECT_LAB_START:
        tracker.expect_lab(LabTransitionAction::START, "5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10");
        return;
    case Step::EXPECT_LAB_STOP:
        tracker.expect_lab(LabTransitionAction::STOP, "5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10");
        return;
    case Step::CANCEL_LAB:
        tracker.cancel_lab();
        return;
    case Step::RUNNING:
        tracker.on_state_change(ProgramState::RUNNING, now);
        return;
    case Step::STOPPING:
        tracker.on_state_change(ProgramState::STOPPING, now);
        return;
    case Step::STOPPED:
        tracker.on_state_change(ProgramState::STOPPED, now);
        return;
    case Step::ERROR_REPORTED:
        tracker.on_error();
        return;
    }
}
UnitTestResult check(const TransitionCase& c){
    LabTransitionTracker tracker;
    WallClock now = utc(2026, 11, 14, hms(6, 0, 0, 0));
    for (Step step : c.input.steps){
        now += 1s;
        apply(tracker, step, now);
    }
    const std::optional<LabTransition> last = tracker.last();
    if (!c.expectation.action){
        return last ? UnitTestResult("expected no transition") : UnitTestResult(true);
    }
    if (!last || last->action != *c.expectation.action || last->source != c.expectation.source){
        return "wrong action or source";
    }
    if (last->request_id.has_value() != c.expectation.has_request_id){
        return "a request id goes with a lab source only";
    }
    return true;
}
class Test_LabTransition : public UnitTest{
public:
    explicit Test_LabTransition(TransitionCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateJson - transition - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    TransitionCase m_case;
};
const TransitionCase TRANSITION_CASES[] = {
    {.name = "nothing yet", .input = {.steps = {}}, .expectation = {.action = std::nullopt}},
    {
        .name = "UI start",
        .input = {.steps = {Step::RUNNING}},
        .expectation = {.action = LabTransitionAction::START, .source = LabTransitionSource::UI},
    },
    {
        .name = "lab start",
        .input = {.steps = {Step::EXPECT_LAB_START, Step::RUNNING}},
        .expectation = {.action = LabTransitionAction::START, .source = LabTransitionSource::LAB, .has_request_id = true},
    },
    {
        .name = "UI stop after a lab start",
        .input = {.steps = {Step::EXPECT_LAB_START, Step::RUNNING, Step::STOPPING, Step::STOPPED}},
        .expectation = {.action = LabTransitionAction::STOP, .source = LabTransitionSource::UI},
    },
    {
        .name = "lab stop",
        .input = {.steps = {Step::RUNNING, Step::EXPECT_LAB_STOP, Step::STOPPING, Step::STOPPED}},
        .expectation = {.action = LabTransitionAction::STOP, .source = LabTransitionSource::LAB, .has_request_id = true},
    },
    {
        .name = "program finished by itself",
        .input = {.steps = {Step::RUNNING, Step::STOPPED}},
        .expectation = {.action = LabTransitionAction::STOP, .source = LabTransitionSource::PROGRAM},
    },
    {
        .name = "program ended with an error",
        .input = {.steps = {Step::RUNNING, Step::ERROR_REPORTED, Step::STOPPED}},
        .expectation = {.action = LabTransitionAction::STOP, .source = LabTransitionSource::PROGRAM_ERROR},
    },
    {
        .name = "error of an earlier run is forgotten",
        .input = {.steps = {Step::RUNNING, Step::ERROR_REPORTED, Step::STOPPED, Step::RUNNING, Step::STOPPED}},
        .expectation = {.action = LabTransitionAction::STOP, .source = LabTransitionSource::PROGRAM},
    },
    {
        .name = "refused lab start, then UI start",
        .input = {.steps = {Step::EXPECT_LAB_START, Step::CANCEL_LAB, Step::RUNNING}},
        .expectation = {.action = LabTransitionAction::START, .source = LabTransitionSource::UI},
    },
    {
        .name = "a lab stop is not taken for a start",
        .input = {.steps = {Step::EXPECT_LAB_STOP, Step::RUNNING}},
        .expectation = {.action = LabTransitionAction::START, .source = LabTransitionSource::UI},
    },
};


}  // namespace


void add_tests_LabStateJson(UnitTestDatabase& database){
    for (const GoldenCase& c : GOLDEN_CASES){
        database.add<Test_LabStateGolden>(c);
    }
    for (const FieldCase& c : FIELD_CASES){
        database.add<Test_LabStateField>(c);
    }
    for (const VideoCase& c : VIDEO_CASES){
        database.add<Test_LabVideoState>(c);
    }
    for (const TransitionCase& c : TRANSITION_CASES){
        database.add<Test_LabTransition>(c);
    }
}



}  // namespace PokemonAutomation
