/*  Interrupts Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The timed screen-event log, the warp judge and the interrupt classifier,
 *  driven by synthetic scenes: a frame's events at a time.
 *
 */

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Pokemon_Interrupts.h"
#include "Pokemon_Interrupts_Tests.h"
#include "Pokemon_ScreenEvents.h"
#include "Pokemon_TestExpect.h"

namespace PokemonAutomation{
namespace Pokemon{

namespace{

using namespace std::chrono_literals;

const WallClock T0 = WallClock() + std::chrono::hours(1);

//  One frame of a synthetic scene: its events, at `ms` after T0.
struct Frame{
    int ms;
    ScreenEvents events;
};

//  A log of `frames`, then the same as the last one every 500 ms until
//  `until_ms` (a walk's screen, looked at while nothing changes).
ScreenEventLog log_of(const std::vector<Frame>& frames, int until_ms, size_t capacity = SCREEN_EVENT_LOG_CAPACITY){
    ScreenEventLog log(capacity);
    for (const Frame& frame : frames){
        log.record(frame.events, T0 + Milliseconds(frame.ms));
    }
    const ScreenEvents last = frames.empty() ? ScreenEvents() : frames.back().events;
    for (int ms = frames.empty() ? 0 : frames.back().ms + 500; ms <= until_ms; ms += 500){
        log.record(last, T0 + Milliseconds(ms));
    }
    return log;
}

const ScreenEvents WALK{ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents BLACK{ScreenEvent::FADE_BLACK};
const ScreenEvents WHITE{ScreenEvent::FADE_WHITE};
const ScreenEvents BOX{ScreenEvent::DIALOG_BOX, ScreenEvent::PLAYER_CAN_MOVE};
const ScreenEvents BATTLE{ScreenEvent::BATTLE_STARTED};
const ScreenEvents BATTLE_TEXT{ScreenEvent::DIALOG_BOX};
const ScreenEvents BANNER{ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE};

InterruptWindow whole(int until_ms){
    return {T0, T0 + Milliseconds(until_ms)};
}


UnitTestResult test_log_records_rises_and_falls(){
    const ScreenEventLog log = log_of({
        {0, WALK},
        {100, WALK},
        {200, BLACK},
        {450, ScreenEvents{ScreenEvent::LOCATION_BANNER, ScreenEvent::PLAYER_CAN_MOVE}},
    }, 450);
    if (log.showing() != BANNER || log.last_frame() != T0 + 450ms || log.dropped() != 0){
        return "the log's last frame is not the banner frame at +450 ms";
    }
    return expect_equal("describe(log)", describe(log, T0),
        "+0 ms player can move rose\n"
        "+200 ms black fade rose\n"
        "+200 ms player can move fell\n"
        "+450 ms black fade fell\n"
        "+450 ms location banner rose\n"
        "+450 ms player can move rose\n"
    );
}

UnitTestResult test_log_overflow_and_clear(){
    ScreenEventLog log = log_of({{0, WALK}, {100, BOX}, {200, WALK}, {300, BOX}, {400, WALK}}, 400, 3);
    if (log.changes().size() != 3 || log.dropped() != 2){
        return std::format("expected 3 kept and 2 dropped, got {} and {}", log.changes().size(), log.dropped());
    }
    if (log.changes().front() != ScreenEventChange{T0 + 200ms, ScreenEvent::DIALOG_BOX, ScreenEventEdge::FELL}){
        return "the oldest change kept is not the box falling at +200 ms";
    }
    const InterruptVerdict early = classify_interrupt(log, whole(400), {});
    const InterruptVerdict late = classify_interrupt(log, {T0 + 250ms, T0 + 400ms}, {});
    if (!early.log_truncated || late.log_truncated){
        return "only a window reaching back past the kept changes is truncated";
    }
    UnitTestResult result = expect_equal("truncated verdict", describe(early, T0), "dialog: +300 ms dialog box rose (log truncated)");
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    log.clear();
    if (!log.changes().empty() || log.dropped() != 0 || log.last_frame() || log.showing() != ScreenEvents()){
        return "clear() did not empty the log";
    }
    return true;
}

UnitTestResult test_empty_log(){
    const ScreenEventLog log;
    const InterruptVerdict verdict = classify_interrupt(log, whole(1000), {});
    if (judge_warp(log, whole(1000), {}, SceneChange::DIFFERENT).judgement != WarpJudgement::NO_FADE){
        return "an empty log has a fade";
    }
    return expect_equal("empty log", describe(verdict, T0), "none");
}


UnitTestResult test_judge_warp(){
    struct Row{
        std::string name;
        std::vector<Frame> frames;
        SceneChange change;
        WarpJudgement expected;
    };
    const std::vector<Row> rows = {
        {"down, held, back up on another scene", {{0, WALK}, {1000, BLACK}, {1400, WALK}}, SceneChange::DIFFERENT, WarpJudgement::WARPED},
        {"no frame compare does not rule it out", {{0, WALK}, {1000, BLACK}, {1400, WALK}}, SceneChange::UNKNOWN, WarpJudgement::WARPED},
        {"back on the same scene", {{0, WALK}, {1000, BLACK}, {1400, WALK}}, SceneChange::SAME, WarpJudgement::SAME_SCENE},
        {"a flicker", {{0, WALK}, {1000, BLACK}, {1050, WALK}}, SceneChange::DIFFERENT, WarpJudgement::TOO_SHORT},
        {"still dark", {{0, WALK}, {1000, BLACK}}, SceneChange::UNKNOWN, WarpJudgement::FADE_SHOWING},
        {"no fade", {{0, WALK}, {1000, BOX}}, SceneChange::DIFFERENT, WarpJudgement::NO_FADE},
    };
    const InterruptKnobs knobs{.min_fade_hold = 200ms};
    for (const Row& row : rows){
        const ScreenEventLog log = log_of(row.frames, 3000);
        const WarpJudgement got = judge_warp(log, whole(3000), knobs, row.change).judgement;
        if (got != row.expected){
            return std::format("{}: expected {}, got {}", row.name, warp_judgement_name(row.expected), warp_judgement_name(got));
        }
    }
    return true;
}


//  Replays a fixed list of event sets, one per frame.
class ScriptedObserver : public ScreenEventObserver{
public:
    explicit ScriptedObserver(std::vector<ScreenEvents> frames)
        : m_frames(std::move(frames))
    {}
    virtual ScreenEvents observe(const VideoSnapshot&, WallClock) override{
        return m_next < m_frames.size() ? m_frames[m_next++] : ScreenEvents();
    }
    virtual void make_overlays(VideoOverlaySet&) const override{}

private:
    std::vector<ScreenEvents> m_frames;
    size_t m_next = 0;
};

UnitTestResult test_watcher_log(){
    ScriptedObserver observer({WALK, WHITE, WALK});
    ScreenEventWatcher watcher(observer);
    for (int c = 0; c < 3; c++){
        watcher.process_frame(VideoSnapshot(ImageRGB32(4, 4), T0 + Milliseconds(100 * c)));
    }
    UnitTestResult result = expect_equal("watcher log", describe(watcher.log(), T0),
        "+0 ms player can move rose\n"
        "+100 ms white fade rose\n"
        "+100 ms player can move fell\n"
        "+200 ms white fade fell\n"
        "+200 ms player can move rose\n"
    );
    if (result.result != UnitTestResult::PASSED){
        return result;
    }
    watcher.reset();
    return expect_equal("watcher log after reset()", describe(watcher.log(), T0), "no changes");
}


struct InterruptsCase{
    struct Input{
        std::string name;
        UnitTestResult (*body)();
    };
    struct Expectation{
    };

    Input input;
    Expectation expectation;
};

class Test_Interrupts : public UnitTest{
public:
    explicit Test_Interrupts(InterruptsCase test_case)
        : UnitTest(std::format("Pokemon::Interrupts - {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    InterruptsCase m_case;
};


struct ClassifyCase{
    //  A synthetic scene, and the window of it to judge.
    struct Input{
        std::string name;
        std::vector<Frame> frames;
        int until_ms = 0;
        InterruptKnobs knobs = {};
        int window_from_ms = 0;
    };
    struct Expectation{
        //  describe(verdict, T0).
        std::string verdict;
    };

    Input input;
    Expectation expectation;
};

class Test_InterruptClassify : public UnitTest{
public:
    explicit Test_InterruptClassify(ClassifyCase test_case)
        : UnitTest(std::format("Pokemon::Interrupts - classify: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ClassifyCase::Input& input = m_case.input;
        const ScreenEventLog log = log_of(input.frames, input.until_ms);
        const InterruptWindow window{T0 + Milliseconds(input.window_from_ms), T0 + Milliseconds(input.until_ms)};
        const InterruptVerdict verdict = classify_interrupt(log, window, input.knobs);
        return expect_equal("verdict", describe(verdict, T0), m_case.expectation.verdict);
    }

private:
    ClassifyCase m_case;
};


struct StepTimeCase{
    //  A synthetic scene, classified over all of it, and the lags to time it.
    struct Input{
        std::string name;
        std::vector<Frame> frames;
        int until_ms = 0;
        std::vector<EventLag> lags;
    };
    struct Expectation{
        //  "+700 ms" from T0, or "none".
        std::string step;
    };

    Input input;
    Expectation expectation;
};

class Test_InterruptStepTime : public UnitTest{
public:
    explicit Test_InterruptStepTime(StepTimeCase test_case)
        : UnitTest(std::format("Pokemon::Interrupts - step time: {}", test_case.input.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const StepTimeCase::Input& input = m_case.input;
        const InterruptVerdict verdict = classify_interrupt(log_of(input.frames, input.until_ms), whole(input.until_ms), {});
        const std::optional<WallClock> step = interrupt_step_time(verdict, input.lags);
        const std::string got = step
            ? std::format("{:+} ms", std::chrono::duration_cast<Milliseconds>(*step - T0).count())
            : "none";
        return expect_equal(std::format("step time of {}", describe(verdict, T0)), got, m_case.expectation.step);
    }

private:
    StepTimeCase m_case;
};

} // namespace


const InterruptsCase INTERRUPTS_CASES[] = {
    {
        .input = {"the log keeps rises and falls in order, timed", test_log_records_rises_and_falls},
        .expectation = {},
    },
    {
        .input = {"a full log drops its oldest changes and says so", test_log_overflow_and_clear},
        .expectation = {},
    },
    {
        .input = {"an empty log is no interrupt and no fade", test_empty_log},
        .expectation = {},
    },
    {
        .input = {"a warp is down, held, back up, not the same scene", test_judge_warp},
        .expectation = {},
    },
    {
        .input = {"a watcher keeps a timed log", test_watcher_log},
        .expectation = {},
    },
};

const ClassifyCase CLASSIFY_CASES[] = {
    {
        .input = {"an encounter's white flash, then the battle",
            {{0, WALK}, {1000, WHITE}, {1300, ScreenEvents()}, {3000, BATTLE}}, 3000},
        .expectation = {"wild encounter: +1000 ms white fade rose, +3000 ms battle started rose"},
    },
    {
        .input = {"a wild battle's opening text comes after its flash",
            {{0, WALK}, {1000, WHITE}, {1300, BATTLE_TEXT}, {2500, BATTLE}}, 2500},
        .expectation = {"wild encounter: +1000 ms white fade rose, +2500 ms battle started rose"},
    },
    {
        .input = {"a trainer's lines, a pause, the battle's fade",
            {{0, WALK}, {1000, BOX}, {2000, WALK}, {6000, BLACK}, {6400, BATTLE}}, 6400},
        .expectation = {"trainer sight: +1000 ms dialog box rose, +6000 ms black fade rose, +6400 ms battle started rose"},
    },
    {
        .input = {"a trainer's lines, then the battle with no fade seen",
            {{0, WALK}, {1000, BOX}, {2000, BATTLE}}, 2000},
        .expectation = {"trainer sight: +1000 ms dialog box rose, +2000 ms battle started rose"},
    },
    {
        .input = {"a box and no battle", {{0, WALK}, {1000, BOX}}, 20000},
        .expectation = {"dialog: +1000 ms dialog box rose"},
    },
    {
        .input = {"a cave exit's white fade, and no battle for 8 s",
            {{0, WALK}, {1000, WHITE}, {1600, WALK}}, 12000},
        .expectation = {"warp: +1000 ms white fade rose, +1600 ms white fade fell"},
    },
    {
        .input = {"a white fade too recent to rule out a battle",
            {{0, WALK}, {1000, WHITE}, {1600, WALK}}, 4000},
        .expectation = {"unsettled: +1000 ms white fade rose, +1600 ms white fade fell"},
    },
    {
        .input = {"a fade still showing", {{0, WALK}, {1000, BLACK}}, 3000},
        .expectation = {"unsettled: +1000 ms black fade rose"},
    },
    {
        .input = {"a banner after the white settles it at once",
            {{0, WALK}, {1000, WHITE}, {1600, WALK}, {2000, BANNER}}, 2500},
        .expectation = {"warp: +1000 ms white fade rose, +1600 ms white fade fell, +2000 ms location banner rose"},
    },
    {
        .input = {"a box during a fade is the scene cut's",
            {{0, WALK}, {1000, BLACK}, {1300, ScreenEvents{ScreenEvent::FADE_BLACK, ScreenEvent::DIALOG_BOX}},
                {1800, BOX}, {3000, WALK}}, 12000},
        .expectation = {"warp: +1000 ms black fade rose, +1800 ms black fade fell"},
    },
    {
        .input = {"a door's fade long before a battle is not the battle's",
            {{0, WALK}, {500, BLACK}, {900, WALK}, {20000, BATTLE}}, 20000},
        .expectation = {"wild encounter: +20000 ms battle started rose"},
    },
    {
        .input = {"a box up before the window is not this interrupt's",
            {{0, BOX}, {1000, WALK}, {3000, WHITE}, {3300, WALK}, {5000, BATTLE}}, 5000, {}, 500},
        .expectation = {"wild encounter: +3000 ms white fade rose, +5000 ms battle started rose"},
    },
    {
        .input = {"a one-frame flicker is no fade with a hold to meet",
            {{0, WALK}, {1000, BLACK}, {1050, WALK}}, 12000, {.min_fade_hold = 200ms}},
        .expectation = {"none"},
    },
    {
        .input = {"a one-frame fade is a warp by default",
            {{0, WALK}, {1000, BLACK}, {1050, WALK}}, 12000},
        .expectation = {"warp: +1000 ms black fade rose, +1050 ms black fade fell"},
    },
    {
        .input = {"a walk with nothing in it", {{0, WALK}}, 5000},
        .expectation = {"none"},
    },
};

const StepTimeCase STEP_TIME_CASES[] = {
    {
        .input = {"a wild battle's box, its fade untimed",
            {{0, WALK}, {1500, BLACK}, {2000, ScreenEvents()}, {4400, BATTLE}}, 6000,
            {{ScreenEvent::BATTLE_STARTED, 3700ms}}},
        .expectation = {"+700 ms"},
    },
    {
        .input = {"the first timed rise wins",
            {{0, WALK}, {1500, BLACK}, {2000, ScreenEvents()}, {4400, BATTLE}}, 6000,
            {{ScreenEvent::BATTLE_STARTED, 3700ms}, {ScreenEvent::FADE_BLACK, 1000ms}}},
        .expectation = {"+500 ms"},
    },
    {
        .input = {"no lags, no time",
            {{0, WALK}, {4400, BATTLE}}, 6000, {}},
        .expectation = {"none"},
    },
    {
        .input = {"no interrupt, no time",
            {{0, WALK}}, 6000, {{ScreenEvent::BATTLE_STARTED, 3700ms}}},
        .expectation = {"none"},
    },
};

void add_tests_Interrupts(UnitTestDatabase& database){
    for (const auto& item : INTERRUPTS_CASES){
        database.add<Test_Interrupts>(item);
    }
    for (const auto& item : CLASSIFY_CASES){
        database.add<Test_InterruptClassify>(item);
    }
    for (const auto& item : STEP_TIME_CASES){
        database.add<Test_InterruptStepTime>(item);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
