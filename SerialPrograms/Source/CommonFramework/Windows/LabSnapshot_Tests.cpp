/*  Lab Snapshot - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  "snapshot LABEL [TIMEOUT_MS]" with no app: its arguments, the frame
 *  counter's wait for a frame newer than the call, and the journal lines the
 *  lab server parses. The instance id and timestamps are those of the
 *  app_state goldens (lab/server/contracts/v1 on the fork's agents branch,
 *  fork PR #478), so a reply names the same frame as state.json.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "LabFrames.h"
#include "LabSnapshot.h"
#include "LabSnapshot_Tests.h"

namespace PokemonAutomation{

namespace{

using namespace std::chrono_literals;


constexpr std::string_view INSTANCE = "d8ea6a9d-1eab-4a40-b355-d88072b289a1";

WallClock utc(int year, unsigned month, unsigned day, Milliseconds time_of_day){
    const std::chrono::sys_days date = std::chrono::year{year} / std::chrono::month{month} / std::chrono::day{day};
    return WallClock(std::chrono::duration_cast<WallDuration>(date.time_since_epoch() + time_of_day));
}


//  parse_lab_snapshot()
struct ParseInput{
    std::vector<std::string> words;
};
struct ParseExpectation{
    std::optional<LabSnapshotArgs> args;    //  None: refused.
};
struct ParseCase{
    std::string name;
    ParseInput input;
    ParseExpectation expectation;
};
class Test_LabSnapshotParse : public UnitTest{
public:
    explicit Test_LabSnapshotParse(ParseCase test_case)
        : UnitTest(std::format("CommonFramework::LabSnapshot - parse - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::expected<LabSnapshotArgs, std::string> args = parse_lab_snapshot(m_case.input.words);
        const std::optional<LabSnapshotArgs>& expected = m_case.expectation.args;
        if (!expected){
            if (args){
                return "expected a refusal";
            }
            return args.error().starts_with("snapshot: ") ? UnitTestResult(true) : UnitTestResult("error names no command");
        }
        if (!args){
            return "refused: " + args.error();
        }
        if (args->label != expected->label || args->timeout != expected->timeout){
            return std::format("got label {} timeout {} ms", args->label, args->timeout.count());
        }
        return true;
    }

private:
    ParseCase m_case;
};
const ParseCase PARSE_CASES[] = {
    {
        .name = "label only",
        .input = {.words = {"snapshot", "frame"}},
        .expectation = {.args = LabSnapshotArgs{.label = "frame", .timeout = LabSnapshotArgs::TIMEOUT_DEFAULT}},
    },
    {
        .name = "label and timeout",
        .input = {.words = {"snapshot", "palab_shot-1", "2500"}},
        .expectation = {.args = LabSnapshotArgs{.label = "palab_shot-1", .timeout = 2500ms}},
    },
    {
        .name = "shortest timeout",
        .input = {.words = {"snapshot", "frame", "100"}},
        .expectation = {.args = LabSnapshotArgs{.label = "frame", .timeout = 100ms}},
    },
    {
        .name = "longest timeout",
        .input = {.words = {"snapshot", "frame", "60000"}},
        .expectation = {.args = LabSnapshotArgs{.label = "frame", .timeout = 60000ms}},
    },
    {.name = "no label", .input = {.words = {"snapshot"}}, .expectation = {.args = std::nullopt}},
    {.name = "extra word", .input = {.words = {"snapshot", "frame", "100", "x"}}, .expectation = {.args = std::nullopt}},
    {.name = "label with a path", .input = {.words = {"snapshot", "../frame"}}, .expectation = {.args = std::nullopt}},
    {.name = "label with a dot", .input = {.words = {"snapshot", "frame.png"}}, .expectation = {.args = std::nullopt}},
    {
        .name = "label too long",
        .input = {.words = {"snapshot", std::string(65, 'a')}},
        .expectation = {.args = std::nullopt},
    },
    {.name = "timeout too short", .input = {.words = {"snapshot", "frame", "99"}}, .expectation = {.args = std::nullopt}},
    {.name = "timeout too long", .input = {.words = {"snapshot", "frame", "60001"}}, .expectation = {.args = std::nullopt}},
    {.name = "timeout not a number", .input = {.words = {"snapshot", "frame", "5s"}}, .expectation = {.args = std::nullopt}},
    {.name = "negative timeout", .input = {.words = {"snapshot", "frame", "-500"}}, .expectation = {.args = std::nullopt}},
};


//  LabFrames::wait_for_next_frame()
enum class During{
    NOTHING,        //  No frame comes.
    FRAME,          //  One frame comes during the wait.
    CANCEL,         //  The app closes.
};
struct WaitInput{
    size_t frames_before = 0;   //  Frames counted before the call.
    During during = During::NOTHING;
    Milliseconds timeout = 0ms;
};
struct WaitExpectation{
    uint64_t after = 0;
    std::optional<uint64_t> frame_seq;  //  None: no frame.
    bool cancelled = false;
};
struct WaitCase{
    std::string name;
    WaitInput input;
    WaitExpectation expectation;
};
class Test_LabSnapshotWait : public UnitTest{
public:
    explicit Test_LabSnapshotWait(WaitCase test_case)
        : UnitTest(std::format("CommonFramework::LabSnapshot - wait - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        LabFrames frames;
        const WallClock start = current_time();
        for (size_t c = 0; c < m_case.input.frames_before; c++){
            frames.on_frame(start, nullptr);
        }
        //  The callback runs during the wait, so a frame it adds came after the call.
        const During during = m_case.input.during;
        const LabFrameWait wait = frames.wait_for_next_frame(
            start + m_case.input.timeout, 10ms,
            [&]{
                if (during == During::FRAME){
                    frames.on_frame(current_time(), nullptr);
                }
                return during == During::CANCEL;
            }
        );
        if (wait.after != m_case.expectation.after){
            return std::format("after {}, expected {}", wait.after, m_case.expectation.after);
        }
        if (wait.cancelled != m_case.expectation.cancelled){
            return "wrong cancelled";
        }
        const std::optional<uint64_t> seq = wait.frame ? std::optional<uint64_t>(wait.frame->token.seq) : std::nullopt;
        if (seq != m_case.expectation.frame_seq){
            return std::format("frame_seq {}", seq ? std::to_string(*seq) : "none");
        }
        const std::optional<LabFrameToken> latest = frames.latest();
        if (seq && (!latest || latest->seq != *seq)){
            return "the snapshot's frame is not state.json's newest";
        }
        return true;
    }

private:
    WaitCase m_case;
};
const WaitCase WAIT_CASES[] = {
    {
        .name = "frame during the wait",
        .input = {.frames_before = 3, .during = During::FRAME, .timeout = 1000ms},
        .expectation = {.after = 3, .frame_seq = 4},
    },
    {
        .name = "first frame of the app",
        .input = {.frames_before = 0, .during = During::FRAME, .timeout = 1000ms},
        .expectation = {.after = 0, .frame_seq = 1},
    },
    {
        .name = "frames before the call are not newer",
        .input = {.frames_before = 3, .during = During::NOTHING, .timeout = 50ms},
        .expectation = {.after = 3, .frame_seq = std::nullopt},
    },
    {
        .name = "no frame at all",
        .input = {.frames_before = 0, .during = During::NOTHING, .timeout = 50ms},
        .expectation = {.after = 0, .frame_seq = std::nullopt},
    },
    {
        .name = "app closing",
        .input = {.frames_before = 2, .during = During::CANCEL, .timeout = 1000ms},
        .expectation = {.after = 2, .frame_seq = std::nullopt, .cancelled = true},
    },
};


//  The journal lines.
struct ReplyInput{
    std::function<LabSnapshotReply()> make;
};
struct ReplyExpectation{
    std::string line;
    std::string error;
};
struct ReplyCase{
    std::string name;
    ReplyInput input;
    ReplyExpectation expectation;
};
class Test_LabSnapshotReply : public UnitTest{
public:
    explicit Test_LabSnapshotReply(ReplyCase test_case)
        : UnitTest(std::format("CommonFramework::LabSnapshot - reply - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const LabSnapshotReply reply = m_case.input.make();
        if (reply.line != m_case.expectation.line){
            return "line: " + reply.line;
        }
        if (reply.error != m_case.expectation.error){
            return "error: " + reply.error;
        }
        return true;
    }

private:
    ReplyCase m_case;
};
const ReplyCase REPLY_CASES[] = {
    {
        .name = "saved",
        .input = {.make = []{
            return lab_snapshot_saved(
                INSTANCE,
                LabFrameToken{.seq = 918275, .captured_at = utc(2026, 11, 14, 6h + 10min + 12s + 124ms)},
                918274,
                "/Users/lab/Library/Application Support/SerialPrograms/Profiles/lab-B/Lab/outbox/r7_frame.png"
            );
        }},
        .expectation = {
            .line =
                "snapshot saved instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 frame_seq=918275"
                " captured_at=2026-11-14T06:10:12.124Z after=918274"
                " path=/Users/lab/Library/Application Support/SerialPrograms/Profiles/lab-B/Lab/outbox/r7_frame.png\n",
            .error = "",
        },
    },
    {
        //  The numbers of contracts/v1/refusal/shot_timeout.json.
        .name = "timeout",
        .input = {.make = []{ return lab_snapshot_timeout(INSTANCE, 918274, 5000ms); }},
        .expectation = {
            .line = "snapshot timeout instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 after=918274 timeout_ms=5000\n",
            .error = "timeout: no frame newer than frame_seq 918274 within 5000 ms",
        },
    },
    {
        .name = "no video",
        .input = {.make = []{ return lab_snapshot_unavailable(INSTANCE, LabSnapshotUnavailable::NO_VIDEO); }},
        .expectation = {
            .line = "snapshot unavailable instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 reason=no_video\n",
            .error = "unavailable: the selected panel has no video",
        },
    },
    {
        .name = "panel changed",
        .input = {.make = []{ return lab_snapshot_unavailable(INSTANCE, LabSnapshotUnavailable::PANEL_CHANGED); }},
        .expectation = {
            .line = "snapshot unavailable instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 reason=panel_changed\n",
            .error = "unavailable: the selected panel changed during the wait",
        },
    },
    {
        .name = "app closing",
        .input = {.make = []{ return lab_snapshot_unavailable(INSTANCE, LabSnapshotUnavailable::APP_CLOSING); }},
        .expectation = {
            .line = "snapshot unavailable instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 reason=app_closing\n",
            .error = "unavailable: the app is closing",
        },
    },
    {
        .name = "save failed",
        .input = {.make = []{ return lab_snapshot_unavailable(INSTANCE, LabSnapshotUnavailable::SAVE_FAILED); }},
        .expectation = {
            .line = "snapshot unavailable instance=d8ea6a9d-1eab-4a40-b355-d88072b289a1 reason=save_failed\n",
            .error = "unavailable: the frame could not be saved",
        },
    },
};


}  // namespace



void add_tests_LabSnapshot(UnitTestDatabase& database){
    for (const ParseCase& c : PARSE_CASES){
        database.add<Test_LabSnapshotParse>(c);
    }
    for (const WaitCase& c : WAIT_CASES){
        database.add<Test_LabSnapshotWait>(c);
    }
    for (const ReplyCase& c : REPLY_CASES){
        database.add<Test_LabSnapshotReply>(c);
    }
}



}  // namespace PokemonAutomation
