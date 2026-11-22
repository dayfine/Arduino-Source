/*  MgbaLink Keys Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "MgbaLink_Keys.h"
#include "MgbaLink_Keys_Tests.h"

namespace PokemonAutomation{
namespace MgbaLink{

namespace{

using std::chrono::milliseconds;

//  Every clock below is anchored at (CLOCK_T0, frame 1000).
const WallClock CLOCK_T0 = WallClock() + std::chrono::hours(1000);
constexpr uint64_t CLOCK_F0 = 1000;



//  Durations to frames.
struct FramesForDurationInput{
    milliseconds duration{};
};
struct FramesForDurationExpectation{
    uint64_t frames = 0;
};
struct FramesForDurationCase{
    std::string name;
    FramesForDurationInput input;
    FramesForDurationExpectation expectation;
};

class Test_FramesForDuration : public UnitTest{
public:
    Test_FramesForDuration(FramesForDurationCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - frames for duration - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        uint64_t frames = frames_for_duration(m_case.input.duration);
        if (frames != m_case.expectation.frames){
            return std::format("{} frame(s), expected {}", frames, m_case.expectation.frames);
        }
        return true;
    }

private:
    FramesForDurationCase m_case;
};



//  App time to the frame it lands on (KEYS) and the frame to grant (GRANT).
struct FrameAtInput{
    milliseconds after_anchor{};
};
struct FrameAtExpectation{
    uint64_t frame = 0;
    uint64_t grant = 0;
};
struct FrameAtCase{
    std::string name;
    FrameAtInput input;
    FrameAtExpectation expectation;
};

class Test_FrameAt : public UnitTest{
public:
    Test_FrameAt(FrameAtCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - frame at time - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        FrameClock clock(CLOCK_T0, CLOCK_F0);
        WallClock time = CLOCK_T0 + m_case.input.after_anchor;
        uint64_t frame = frame_at(clock, time);
        uint64_t grant = grant_frame(clock, time, KeySchedule::LEAD_FRAMES);
        if (frame != m_case.expectation.frame || grant != m_case.expectation.grant){
            return std::format(
                "frame {}, grant {}; expected frame {}, grant {}",
                frame, grant, m_case.expectation.frame, m_case.expectation.grant
            );
        }
        return true;
    }

private:
    FrameAtCase m_case;
};



//  Controller states to KEYS commands. Frame 1002, the first one after the
//  lead, starts 33.5 ms after the anchor.
struct StatePush{
    milliseconds now_after_anchor{};
    uint16_t mask = 0;
    milliseconds duration{};
};
StatePush hold_at(milliseconds now_after_anchor, uint16_t mask, milliseconds duration){
    return StatePush{now_after_anchor, mask, duration};
}
struct KeyScheduleInput{
    std::vector<StatePush> pushes;
};
struct KeyScheduleExpectation{
    std::string commands;
    uint64_t end_frame = 0;
};
struct KeyScheduleCase{
    std::string name;
    KeyScheduleInput input;
    KeyScheduleExpectation expectation;
};

class Test_KeySchedule : public UnitTest{
public:
    Test_KeySchedule(KeyScheduleCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - key schedule - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        KeySchedule schedule(FrameClock(CLOCK_T0, CLOCK_F0));
        std::string commands;
        for (const StatePush& push : m_case.input.pushes){
            std::optional<KeysCommand> command = schedule.push(CLOCK_T0 + push.now_after_anchor, push.mask, push.duration);
            if (command){
                commands += commands.empty() ? "" : ", ";
                commands += std::format("KEYS {} {}", command->frame, command->mask);
            }
        }
        if (commands != m_case.expectation.commands || schedule.end_frame() != m_case.expectation.end_frame){
            return std::format(
                "[{}] ending at {}, expected [{}] ending at {}",
                commands, schedule.end_frame(), m_case.expectation.commands, m_case.expectation.end_frame
            );
        }
        return true;
    }

private:
    KeyScheduleCase m_case;
};



//  Commands sent and how late each landed, to the input log's text.
struct SentKeys{
    KeysCommand command;
    uint64_t frames_late = 0;
};
SentKeys sent_late(uint64_t frame, uint16_t mask, uint64_t frames_late){
    return SentKeys{KeysCommand{frame, mask}, frames_late};
}
struct InputLogInput{
    std::vector<SentKeys> sent;
};
struct InputLogExpectation{
    std::string text;
};
struct InputLogCase{
    std::string name;
    InputLogInput input;
    InputLogExpectation expectation;
};

class Test_InputLog : public UnitTest{
public:
    Test_InputLog(InputLogCase test_case)
        : UnitTest(std::format("Controllers::MgbaLink - input log - {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        InputLog log;
        for (const SentKeys& sent : m_case.input.sent){
            log.record(sent.command, sent.frames_late);
        }
        std::string text = log.to_text();
        if (text != m_case.expectation.text){
            return std::format("logged <{}>", text);
        }
        return true;
    }

private:
    InputLogCase m_case;
};



void add_frame_math_tests(UnitTestDatabase& database){
    const FramesForDurationCase FRAMES_FOR_DURATION_CASES[] = {
        {.name = "zero holds nothing", .input = {milliseconds(0)}, .expectation = {0}},
        {.name = "8 ms is still one frame", .input = {milliseconds(8)}, .expectation = {1}},
        {.name = "17 ms", .input = {milliseconds(17)}, .expectation = {1}},
        {.name = "80 ms", .input = {milliseconds(80)}, .expectation = {5}},
        {.name = "1000 ms", .input = {milliseconds(1000)}, .expectation = {60}},
    };
    for (const FramesForDurationCase& test_case : FRAMES_FOR_DURATION_CASES){
        database.add<Test_FramesForDuration>(test_case);
    }

    //  100 ms is 5.97 frames: rounded for KEYS, floored for GRANT.
    const FrameAtCase FRAME_AT_CASES[] = {
        {.name = "at the anchor", .input = {milliseconds(0)}, .expectation = {1000, 1002}},
        {.name = "100 ms", .input = {milliseconds(100)}, .expectation = {1006, 1007}},
        {.name = "1000 ms", .input = {milliseconds(1000)}, .expectation = {1060, 1061}},
        {.name = "before the anchor", .input = {milliseconds(-100)}, .expectation = {994, 996}},
    };
    for (const FrameAtCase& test_case : FRAME_AT_CASES){
        database.add<Test_FrameAt>(test_case);
    }
}

void add_key_schedule_tests(UnitTestDatabase& database){
    const uint16_t A = GBA_KEY_A;
    const uint16_t B = GBA_KEY_B;
    const milliseconds T0(0), MS50(50);
    const KeyScheduleCase KEY_SCHEDULE_CASES[] = {
        {
            .name = "80 ms press holds 5 frames",
            .input = {{hold_at(T0, A, milliseconds(80)), hold_at(T0, 0, milliseconds(80))}},
            .expectation = {"KEYS 1002 1, KEYS 1007 0", 1012},
        },
        {
            .name = "1000 ms press holds 60 frames",
            .input = {{hold_at(T0, A, milliseconds(1000)), hold_at(T0, 0, MS50)}},
            .expectation = {"KEYS 1002 1, KEYS 1062 0", 1065},
        },
        {
            .name = "17 ms tap holds 1 frame",
            .input = {{hold_at(T0, A, milliseconds(17)), hold_at(T0, 0, milliseconds(17))}},
            .expectation = {"KEYS 1002 1, KEYS 1003 0", 1004},
        },
        {
            .name = "5 ms tap holds 1 frame, the release ends at its own 100 ms",
            .input = {{hold_at(T0, A, milliseconds(5)), hold_at(T0, 0, milliseconds(100))}},
            .expectation = {"KEYS 1002 1, KEYS 1003 0", 1008},
        },
        {
            .name = "same keys again send nothing",
            .input = {{hold_at(T0, A, MS50), hold_at(T0, A, MS50), hold_at(T0, 0, MS50)}},
            .expectation = {"KEYS 1002 1, KEYS 1008 0", 1011},
        },
        {
            .name = "after idle the next state starts the lead after now",
            .input = {{hold_at(T0, A, MS50), hold_at(milliseconds(1000), B, MS50)}},
            .expectation = {"KEYS 1002 1, KEYS 1062 2", 1065},
        },
        {
            .name = "zero duration is dropped",
            .input = {{hold_at(T0, A, milliseconds(0)), hold_at(T0, B, MS50)}},
            .expectation = {"KEYS 1002 2", 1005},
        },
        {
            .name = "first state with no keys is still sent",
            .input = {{hold_at(T0, 0, MS50)}},
            .expectation = {"KEYS 1002 0", 1005},
        },
    };
    for (const KeyScheduleCase& test_case : KEY_SCHEDULE_CASES){
        database.add<Test_KeySchedule>(test_case);
    }
}

void add_input_log_tests(UnitTestDatabase& database){
    const InputLogCase INPUT_LOG_CASES[] = {
        {.name = "empty", .input = {}, .expectation = {""}},
        {.name = "on time", .input = {{sent_late(1002, GBA_KEY_A, 0)}}, .expectation = {"1002 1\n"}},
        {
            .name = "late command logged where it landed",
            .input = {{sent_late(1002, GBA_KEY_A, 0), sent_late(1007, GBA_KEY_NONE, 3)}},
            .expectation = {"1002 1\n1010 0\n"},
        },
    };
    for (const InputLogCase& test_case : INPUT_LOG_CASES){
        database.add<Test_InputLog>(test_case);
    }
}

}  // namespace



void add_key_tests(UnitTestDatabase& database){
    add_frame_math_tests(database);
    add_key_schedule_tests(database);
    add_input_log_tests(database);
}


}  // namespace MgbaLink
}  // namespace PokemonAutomation
