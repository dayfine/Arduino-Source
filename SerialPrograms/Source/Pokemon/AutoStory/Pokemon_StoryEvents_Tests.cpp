/*  Story Events Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StoryEvents_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


//  How a scripted event's action ends.
enum class ScriptedAct{
    PASS,
    PASS_AT_LOBBY,  //  succeeds knowing the player is in "Lobby"
    FAIL,
    THROW,
};

struct EventsCase{
    std::string name;
    struct Input{
        std::vector<ScriptedAct> events;
    };
    Input input;
    struct Expectation{
        //  "completed 2 run, pose Lobby" or "failed at 2 e2: scripted, 2 run".
        std::string report;
        //  The log, one line each, joined by " | "; "throws" when it throws.
        std::string log;
    };
    Expectation expectation;
};

const EventsCase CASES[] = {
    {
        .name = "every event runs in order",
        .input = {.events = {ScriptedAct::PASS, ScriptedAct::PASS}},
        .expectation = {
            .report = "completed, 2 run, pose ?",
            .log = "[Checkpoint] 01.01: start, 2 events | [Event] 01.01/1 e1: start | [Event] 01.01/1 e1: done"
                " | [Event] 01.01/2 e2: start | [Event] 01.01/2 e2: done | [Checkpoint] 01.01: done, 2 run",
        },
    },
    {
        .name = "a failed event stops the checkpoint",
        .input = {.events = {ScriptedAct::PASS, ScriptedAct::FAIL, ScriptedAct::PASS}},
        .expectation = {
            .report = "failed at 2 e2: scripted, 2 run",
            .log = "[Checkpoint] 01.01: start, 3 events | [Event] 01.01/1 e1: start | [Event] 01.01/1 e1: done"
                " | [Event] 01.01/2 e2: start | [Event] 01.01/2 e2: failed: scripted: as scripted"
                " | [Checkpoint] 01.01: failed at 2 e2: scripted: as scripted",
        },
    },
    {
        .name = "an action's pose becomes the run's",
        .input = {.events = {ScriptedAct::PASS_AT_LOBBY}},
        .expectation = {
            .report = "completed, 1 run, pose Lobby",
            .log = "[Checkpoint] 01.01: start, 1 events | [Event] 01.01/1 e1: start | [Event] 01.01/1 e1: done | [Checkpoint] 01.01: done, 1 run",
        },
    },
    {
        .name = "a throw is logged as the event's failure and goes on",
        .input = {.events = {ScriptedAct::THROW}},
        .expectation = {
            .report = "throws",
            .log = "[Checkpoint] 01.01: start, 1 events | [Event] 01.01/1 e1: start | [Event] 01.01/1 e1: failed: boom"
                " | [Checkpoint] 01.01: failed at 1 e1: boom",
        },
    },
};

Act act_of(ScriptedAct kind){
    return [kind](RunState&) -> Outcome{
        switch (kind){
        case ScriptedAct::PASS:
            return outcome_ok();
        case ScriptedAct::PASS_AT_LOBBY:
            return outcome_ok(Pose{.map = "Lobby", .certainty = PoseCertainty::SET, .where = nullptr});
        case ScriptedAct::FAIL:
            return outcome_failed("scripted", "as scripted");
        case ScriptedAct::THROW:
            throw std::runtime_error("boom");
        }
        return outcome_ok();
    };
}

UnitTestResult check_case(const EventsCase& test){
    Checkpoint checkpoint{.id = "01.01", .events = {}};
    for (size_t c = 0; c < test.input.events.size(); c++){
        checkpoint.events.push_back(Event{.name = std::format("e{}", c + 1), .why = "", .act = act_of(test.input.events[c])});
    }
    std::string log;
    EventLogger logger = [&log](const std::string& line){
        log += (log.empty() ? "" : " | ") + line;
    };
    RunState state;
    std::string report = "throws";
    try{
        const CheckpointReport run = run_checkpoint(state, checkpoint, logger);
        report = run.completed
            ? std::format("completed, {} run, pose {}", run.events_run, state.pose.map.empty() ? "?" : state.pose.map)
            : std::format("failed at {} {}: {}, {} run", run.failed_event, run.failed_name, run.failure.failure, run.events_run);
    }catch (const std::runtime_error&){}
    if (report != test.expectation.report){
        return std::format("expected \"{}\", got \"{}\"", test.expectation.report, report);
    }
    if (log != test.expectation.log){
        return std::format("expected the log \"{}\", got \"{}\"", test.expectation.log, log);
    }
    return true;
}


class StoryEventsTest : public UnitTest{
public:
    explicit StoryEventsTest(EventsCase test_case)
        : UnitTest(std::format("Pokemon::StoryEvents - {}", test_case.name))
        , m_case(std::move(test_case))
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return check_case(m_case);
    }

private:
    EventsCase m_case;
};

} // namespace


void add_tests_StoryEvents(UnitTestDatabase& database){
    for (const EventsCase& test : CASES){
        database.add<StoryEventsTest>(test);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
