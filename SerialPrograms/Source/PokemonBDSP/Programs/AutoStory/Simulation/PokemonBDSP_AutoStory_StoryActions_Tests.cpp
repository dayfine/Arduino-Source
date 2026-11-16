/*  BDSP AutoStory - Story actions Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <functional>
#include <set>
#include <string>
#include <string_view>
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

//  Every authored scene keeps the full opening-line guard unless the
//  preceding settle actually observed dialogue.
UnitTestResult test_scene_start_waits(Logger& logger){
    constexpr std::string_view LAST_ROW = "01.11";
    constexpr size_t EXPECTED_SCENES = 23;
    //  The default grid naturally plays Barry's doorway scene. The two
    //  other measured doorway scenes are supplied by the test below.
    constexpr std::string_view DEFAULT_SKIP = "barry_goes_home";
    SimExecutor sim(logger);
    run_rows_on(sim, logger, {.from = "00.00", .to = std::string(LAST_ROW)});
    if (!sim.result().error.empty()){
        return std::format("The run failed: {}", sim.result().error);
    }
    size_t scenes = 0;
    size_t guarded = 0;
    for (const auto& [row, step] : story_steps()){
        if (row > LAST_ROW || step.kind != StoryStepKind::MASH){
            continue;
        }
        const auto found = sim.misses().dialog_waits.find(step.name);
        const bool should_skip = step.name == DEFAULT_SKIP;
        if (found == sim.misses().dialog_waits.end()){
            if (!should_skip){
                return std::format("{} {} skipped its opening-line guard without preceding dialogue.", row, step.name);
            }
        }else{
            if (should_skip){
                return std::format("{} {} repeated its opening-line guard after preceding dialogue.", row, step.name);
            }
            //  Mirrors SCENE_START_WAIT in StoryWalkSteps.cpp.
            if (found->second != Milliseconds(15000)){
                return std::format("{} {} waits {} ms for its opening line; expected the full 15000 ms guard.",
                    row, step.name, found->second.count());
            }
            guarded++;
        }
        scenes++;
    }
    if (scenes != EXPECTED_SCENES){
        return std::format("Audited {} scene steps through {}; expected {}.", scenes, LAST_ROW, EXPECTED_SCENES);
    }
    if (guarded + 1 != scenes){
        return std::format("{} of {} scenes kept the guard; expected one exact skip.", guarded, scenes);
    }
    logger.log(std::format("{} scene steps audited through {}; {} used the full opening-line guard.", scenes, LAST_ROW, guarded));
    return true;
}

//  Console B, 2026-11-15: these arrival scenes had already been played by
//  the preceding settle, so their separate 15 s waits did no work.
UnitTestResult test_seen_scene_skips_start_wait(Logger& logger){
    SimWorldKnobs knobs;
    knobs.scenes["down_the_stairs"] = {sim_line("Mom calls the player")};
    knobs.scenes["barry_bursts_out"] = {sim_line("Barry goes home")};
    knobs.scenes["up_barrys_stairs"] = {sim_line("Barry runs to the lake")};
    SimExecutor sim(logger, /*misses=*/{}, knobs);
    run_rows_on(sim, logger, {.from = "00.01", .to = "00.01"});
    if (!sim.result().error.empty()){
        return std::format("The run failed: {}", sim.result().error);
    }
    const std::pair<std::string_view, std::string_view> expected_skips[] = {
        {"down_the_stairs", "mom_calls"},
        {"barry_bursts_out", "barry_goes_home"},
        {"up_barrys_stairs", "barry_runs_to_the_lake"},
    };
    for (const auto& [previous, scene] : expected_skips){
        if (sim.misses().dialog_waits.contains(std::string(scene))){
            return std::format("{} repeated an opening-line wait after {} already observed dialogue.", scene, previous);
        }
    }
    return true;
}

//  Dialogue seen before an intervening TALK or BATTLE belongs to the step
//  before that action, not to the following scene.
UnitTestResult test_intervening_step_keeps_start_wait(Logger& logger){
    const StoryStepKind intervening_kinds[] = {StoryStepKind::TALK, StoryStepKind::BATTLE};
    for (StoryStepKind kind : intervening_kinds){
        GridSim grid(
            {.world = story_plan_world("TwinleafTown", "TwinleafTown")},
            /*start=*/{0, 0}, Pokemon::Direction::NORTH, /*can_run=*/false
        );
        std::vector<std::string> notes;
        SimMisses misses;
        const std::string io_step = "scene";
        SimWorldKnobs knobs;
        knobs.first_line[io_step] = Milliseconds(0);
        knobs.scenes[io_step] = {sim_line("the scene's line")};
        SimStoryStepIO io(grid, notes, misses, knobs, io_step, []{ return false; }, [](const Pokemon::Pose&){});
        StoryWalkSteps walks(io);
        CancellableHolder<CancellableScope> scope;
        StoryLogBackend backend(Pokemon::StoryBackendKind::SCRIPT, /*log=*/{}, scope);
        StoryBoundActions actions = naming_actions();
        actions.battle = [](const StoryStep&, RunState&){ return Pokemon::outcome_ok(); };
        StoryEngineSteps engine(walks, actions, backend);
        RunState state;

        StoryStep preceding{.kind = StoryStepKind::MASH, .name = "preceding_scene"};
        engine.act(preceding, state);
        misses.dialog_waits.clear();
        StoryStep intervening{.kind = kind, .name = "intervening_step"};
        engine.act(intervening, state);
        misses.dialog_waits.clear();
        StoryStep following{.kind = StoryStepKind::MASH, .name = "following_scene"};
        engine.act(following, state);

        const auto found = misses.dialog_waits.find(io_step);
        if (found == misses.dialog_waits.end() || found->second != Milliseconds(15000)){
            return std::format("A scene after {} did not keep its full opening-line guard.",
                Pokemon::story_step_kind_name(kind));
        }
    }
    logger.log("Scenes after TALK and BATTLE keep their full opening-line guard.");
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
    {
        .input = {"Story actions: scene starts retain the full guard until dialogue is observed", test_scene_start_waits},
        .expectation = {},
    },
    {
        .input = {"Story actions: a scene played by the preceding settle skips its start wait", test_seen_scene_skips_start_wait},
        .expectation = {},
    },
    {
        .input = {"Story actions: an intervening step invalidates a prior settle's dialogue", test_intervening_step_keeps_start_wait},
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
