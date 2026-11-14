/*  BDSP AutoStory - Story actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryActions.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP_AutoStory_GridSim.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_StoryActions_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{
using namespace detail;


namespace{

using Pokemon::RunState;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


//  Every step the story runs: the table's, and each row's save at its start.
std::vector<std::pair<std::string, StoryStep>> story_steps(){
    std::vector<std::pair<std::string, StoryStep>> out;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            out.emplace_back(row.id, step);
        }
        StoryStep save;
        save.kind = StoryStepKind::SAVE;
        save.name = "save";
        save.args.emplace(SAVES_IN_ARG, row.saves_in);
        out.emplace_back(row.id, std::move(save));
    }
    return out;
}

//  What ran a step before the table: the executors' own dispatch. An event
//  named by the whole variant, whatever the kind; else the kind's method,
//  which StoryWalkSteps does for the walking kinds, and the console (or
//  the simulation's stand-in) for the others; else the base class's.
std::string old_dispatch(const StoryStep& step){
    const std::string kind = Pokemon::story_step_kind_name(step.kind);
    if (story_events().contains(step.action)){
        return "event " + step.action;
    }
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::LEGS:
    case StoryStepKind::STEP:
    case StoryStepKind::TALK:
    case StoryStepKind::MASH:
        return std::format("walk {}", kind);
    case StoryStepKind::NEW_GAME:
    case StoryStepKind::OPTIONS:
    case StoryStepKind::ANSWER:
    case StoryStepKind::BATTLE:
    case StoryStepKind::TRAIN:
    case StoryStepKind::HEAL:
    case StoryStepKind::SAVE:
        return "console " + kind;
    default:
        return std::format("executor {}", kind);
    }
}

//  The table's binding that names what it would run, as a failure.
StoryBoundActions naming_actions(){
    const auto named = [](StoryStepKind kind){
        return [kind](const StoryStep&, RunState&){
            return Pokemon::outcome_failed("console", Pokemon::story_step_kind_name(kind));
        };
    };
    return StoryBoundActions{
        .new_game = named(StoryStepKind::NEW_GAME),
        .options = named(StoryStepKind::OPTIONS),
        .answer = named(StoryStepKind::ANSWER),
        .battle = named(StoryStepKind::BATTLE),
        .train = named(StoryStepKind::TRAIN),
        .heal = named(StoryStepKind::HEAL),
        .save = named(StoryStepKind::SAVE),
        .event = [](const std::string& name) -> StoryBoundAction{
            return [name](const StoryStep&, RunState&){
                return Pokemon::outcome_failed("event", name);
            };
        },
    };
}

//  Whether `step` runs one of the binding's own actions, which need the
//  console: an event, or a kind StoryWalkSteps does not do.
bool needs_console(const StoryStep& step){
    if (story_events().contains(step.action)){
        return true;
    }
    switch (step.kind){
    case StoryStepKind::ROUTE:
    case StoryStepKind::LEGS:
    case StoryStepKind::STEP:
    case StoryStepKind::TALK:
    case StoryStepKind::MASH:
        return false;
    default:
        return true;
    }
}

//  What `steps` runs for `step`, in old_dispatch()'s words. A walking step
//  is not run: it would walk.
std::string engine_dispatch(StoryEngineSteps& steps, const StoryStep& step){
    const std::string kind = Pokemon::story_step_kind_name(step.kind);
    if (!steps.check(step).empty()){
        return std::format("executor {}", kind);
    }
    if (!needs_console(step)){
        return std::format("walk {}", kind);
    }
    RunState state;
    const Pokemon::Outcome outcome = steps.act(step, state);
    if (outcome.failure == "unsupported_step"){
        return std::format("executor {}", kind);
    }
    return std::format("{} {}", outcome.failure, outcome.detail);
}


UnitTestResult test_every_step_has_an_action(Logger& logger){
    SimExecutor sim(logger);
    size_t steps = 0;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (size_t c = 0; c < row.steps.size(); c++){
            const StoryStep& step = row.steps[c];
            const std::vector<std::string> problems = sim.steps().check(step);
            if (!problems.empty()){
                return std::format(
                    "{} step {} {}: {} for {} '{}'",
                    row.id, c + 1, step.name, problems.front(), Pokemon::story_step_kind_name(step.kind), step.action
                );
            }
        }
        steps += row.steps.size();
    }
    logger.log(std::format("{} rows, {} steps, each with an action.", story_checkpoints().size(), steps));
    return true;
}

UnitTestResult test_table_runs_what_the_old_dispatch_ran(Logger& logger){
    GridSim sim({}, {0, 0}, Pokemon::Direction::NORTH, false);
    std::vector<std::string> notes;
    SimMisses misses;
    const SimWorldKnobs knobs;
    const std::string step_name;
    SimStoryStepIO io(sim, notes, misses, knobs, step_name, []{ return false; }, [](const Pokemon::Pose&){});
    StoryWalkSteps walks(io);
    CancellableHolder<CancellableScope> scope;
    StoryLogBackend backend(Pokemon::StoryBackendKind::SCRIPT, {}, scope);
    StoryEngineSteps engine(walks, naming_actions(), backend);

    std::set<std::pair<StoryStepKind, std::string>> seen;
    for (const auto& [row, step] : story_steps()){
        if (!seen.emplace(step.kind, step.action).second){
            continue;
        }
        const std::string expected = old_dispatch(step);
        const std::string actual = engine_dispatch(engine, step);
        if (actual != expected){
            return std::format("{} {} ({} \"{}\"): the engine runs \"{}\", the old dispatch ran \"{}\".",
                row, step.name, Pokemon::story_step_kind_name(step.kind), step.action, actual, expected);
        }
    }
    logger.log(std::format("{} (kind, action) pairs run as before.", seen.size()));
    return true;
}

UnitTestResult test_stubbed_steps_need_the_console(Logger& logger){
    SimExecutor sim(logger);
    run_rows_on(sim, logger, {.from = "00.00", .to = "01.11"});
    logger.log("Steps the simulation does not play, by row:\n" + stub_report(sim.result()));
    for (const auto& [row, step] : story_steps()){
        if (needs_console(step)){
            continue;
        }
        for (const SimRun::RowStart& start : sim.result().rows){
            if (start.row != row){
                continue;
            }
            for (const std::string& stubbed : start.stubbed){
                if (stubbed == step.name){
                    return std::format("{} {}: stubbed, but the simulation plays its kind.", row, step.name);
                }
            }
        }
    }
    if (!sim.result().error.empty()){
        return "The run failed: " + sim.result().error;
    }
    return true;
}



struct AutoStoryStoryActionsInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryStoryActionsExpectation{
};

struct AutoStoryStoryActionsCase{
    AutoStoryStoryActionsInput input;
    AutoStoryStoryActionsExpectation expectation;
};

class Test_AutoStoryStoryActions : public UnitTest{
public:
    explicit Test_AutoStoryStoryActions(AutoStoryStoryActionsCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStoryStoryActionsCase m_case;
};

} // namespace


const AutoStoryStoryActionsCase AUTO_STORY_STORY_ACTIONS_CASES[] = {
    {
        .input = {"Story actions: every step of every row has an action", test_every_step_has_an_action},
        .expectation = {},
    },
    {
        .input = {"Story actions: each (kind, action) runs what the executors' own dispatch ran", test_table_runs_what_the_old_dispatch_ran},
        .expectation = {},
    },
    {
        .input = {"Story actions: the steps the sim stubs from 00.00 to 01.11 all need the console", test_stubbed_steps_need_the_console},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryActions(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_ACTIONS_CASES){
        database.add<Test_AutoStoryStoryActions>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
