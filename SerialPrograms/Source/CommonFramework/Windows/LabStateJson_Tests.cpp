/*  Lab State JSON - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "Lab/state.json" against the lab server's version-1 goldens, the video
 *  state, which transition it names, and when it is rewritten.
 *
 *  The goldens are copies of lab/server/contracts/v1/app_state/{stopped_ui,
 *  running_lab,no_program}.json on the fork's agents branch (fork PR #478 at
 *  ff209b333). Change them only together with the contract.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabStateJson.h"
#include "LabTransition.h"
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
    state.last_transition = ProgramTransition{
        .action = ProgramAction::STOP, .source = ActionSource::UI,
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
    state.last_transition = ProgramTransition{
        .action = ProgramAction::START, .source = ActionSource::LAB,
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
    std::optional<std::string> options_hash = {};
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
    state.options_hash = c.input.options_hash;
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
    {
        .name = "the program's options hash",
        .input = {.options_hash = "ab12"},
        .expectation = {.lines = {"\"last_transition\": {", "\"options_hash\": \"ab12\"\n  },\n  \"controller\""}},
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


//  lab_transition_since()
struct SinceInput{
    std::optional<Milliseconds> transition_at;  //  From the selection; none: no transition yet.
};
struct SinceExpectation{
    bool kept;
};
struct SinceCase{
    std::string name;
    SinceInput input;
    SinceExpectation expectation;
};
UnitTestResult check(const SinceCase& c){
    const WallClock selected_at = utc(2026, 11, 14, hms(6, 0, 0, 0));
    std::optional<ProgramTransition> last;
    if (c.input.transition_at){
        last = ProgramTransition{
            .action = ProgramAction::START, .source = ActionSource::UI,
            .request_id = std::nullopt, .at = selected_at + *c.input.transition_at,
        };
    }
    const bool kept = lab_transition_since(last, selected_at).has_value();
    if (kept != c.expectation.kept){
        return std::format("expected kept={}", c.expectation.kept);
    }
    return true;
}
class Test_LabTransitionSince : public UnitTest{
public:
    explicit Test_LabTransitionSince(SinceCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateJson - transition - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    SinceCase m_case;
};
const SinceCase SINCE_CASES[] = {
    {.name = "nothing yet", .input = {.transition_at = std::nullopt}, .expectation = {.kept = false}},
    {.name = "after the selection", .input = {.transition_at = 1500ms}, .expectation = {.kept = true}},
    {.name = "at the selection", .input = {.transition_at = 0ms}, .expectation = {.kept = true}},
    {
        .name = "transition of an earlier selection is forgotten",
        .input = {.transition_at = -1500ms},
        .expectation = {.kept = false},
    },
};


//  What changed in the state since the previous call to LabStateJsonThrottle.
enum class Change{
    NOTHING,            //  Only the heartbeat.
    NEW_FRAME,
    PROGRAM_STARTED,
    REQUEST_FINISHED,
    VIDEO_STALE,
    OPTIONS_CHANGED,
    APP_RESTARTED,
    FILE_GONE,
};
struct ThrottleCall{
    Milliseconds after;     //  Since the previous call.
    Change change;
};
struct ThrottleInput{
    std::vector<ThrottleCall> calls;
};
struct ThrottleExpectation{
    std::vector<bool> written;  //  Per call.
};
struct ThrottleCase{
    std::string name;
    ThrottleInput input;
    ThrottleExpectation expectation;
};
void apply(Change change, LabAppState& state, LabStateJsonThrottle& throttle){
    switch (change){
    case Change::NOTHING:
        return;
    case Change::NEW_FRAME:
        state.frame->seq++;
        state.frame->captured_at = state.written_at;
        return;
    case Change::PROGRAM_STARTED:
        state.legacy.state = "RUNNING";
        state.last_transition = ProgramTransition{
            .action = ProgramAction::START, .source = ActionSource::LAB,
            .request_id = "5f0c2a8e-3d41-4b7a-9c55-0e6a1f2b7c10", .at = state.written_at,
        };
        return;
    case Change::REQUEST_FINISHED:
        state.legacy.seq += "+";
        return;
    case Change::VIDEO_STALE:
        state.video = LabVideoState::STALE;
        return;
    case Change::OPTIONS_CHANGED:
        state.options_hash = "cd34";
        return;
    case Change::APP_RESTARTED:
        state.instance_id = "0c7b3f52-91d4-4e8a-a3f6-5b2e7d10c9a4";
        state.frame->seq = 1;
        return;
    case Change::FILE_GONE:
        throttle.reset();
        return;
    }
}
UnitTestResult check(const ThrottleCase& c){
    LabStateJsonThrottle throttle(1000ms);
    LabAppState state = stopped_ui();
    std::vector<bool> written;
    for (const ThrottleCall& call : c.input.calls){
        state.written_at += call.after;
        apply(call.change, state, throttle);
        const std::optional<std::string> text = throttle.text_to_write(state);
        if (text && *text != lab_state_json_text(state)){
            return "a write must carry the newest state";
        }
        written.push_back(text.has_value());
    }
    if (written != c.expectation.written){
        std::string got;
        for (bool w : written){
            got += w ? "w" : "-";
        }
        return std::format("wrong writes: {}", got);
    }
    return true;
}
class Test_LabStateThrottle : public UnitTest{
public:
    explicit Test_LabStateThrottle(ThrottleCase test_case)
        : UnitTest(std::format("CommonFramework::LabStateJson - throttle - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check(m_case);
    }

private:
    ThrottleCase m_case;
};
const ThrottleCase THROTTLE_CASES[] = {
    {
        .name = "first state is written",
        .input = {.calls = {{0ms, Change::NOTHING}}},
        .expectation = {.written = {true}},
    },
    {
        .name = "heartbeat at most once per interval",
        .input = {.calls = {{0ms, Change::NOTHING}, {500ms, Change::NOTHING}, {500ms, Change::NOTHING}}},
        .expectation = {.written = {true, false, true}},
    },
    {
        .name = "frames coalesce, the newest is written",
        .input = {.calls = {
            {0ms, Change::NOTHING}, {100ms, Change::NEW_FRAME}, {100ms, Change::NEW_FRAME}, {900ms, Change::NEW_FRAME},
        }},
        .expectation = {.written = {true, false, false, true}},
    },
    {
        .name = "start is written at once",
        .input = {.calls = {{0ms, Change::NOTHING}, {100ms, Change::PROGRAM_STARTED}}},
        .expectation = {.written = {true, true}},
    },
    {
        .name = "finished request is written at once",
        .input = {.calls = {{0ms, Change::NOTHING}, {100ms, Change::REQUEST_FINISHED}}},
        .expectation = {.written = {true, true}},
    },
    {
        .name = "stale video is written at once",
        .input = {.calls = {{0ms, Change::NOTHING}, {100ms, Change::VIDEO_STALE}}},
        .expectation = {.written = {true, true}},
    },
    {
        .name = "an options change is written at once",
        .input = {.calls = {{0ms, Change::NOTHING}, {100ms, Change::OPTIONS_CHANGED}}},
        .expectation = {.written = {true, true}},
    },
    {
        .name = "restart is written at once",
        .input = {.calls = {{0ms, Change::NOTHING}, {100ms, Change::APP_RESTARTED}}},
        .expectation = {.written = {true, true}},
    },
    {
        .name = "missing file is written at once",
        .input = {.calls = {{0ms, Change::NOTHING}, {100ms, Change::FILE_GONE}}},
        .expectation = {.written = {true, true}},
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
    for (const SinceCase& c : SINCE_CASES){
        database.add<Test_LabTransitionSince>(c);
    }
    for (const ThrottleCase& c : THROTTLE_CASES){
        database.add<Test_LabStateThrottle>(c);
    }
}



}  // namespace PokemonAutomation
