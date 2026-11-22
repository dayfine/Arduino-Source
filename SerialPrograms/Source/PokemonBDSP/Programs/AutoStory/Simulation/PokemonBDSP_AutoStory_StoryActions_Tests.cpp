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
#include "Pokemon/AutoStory/Data/Pokemon_StoryCheckpointJson.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryActions.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryData.h"
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

using Pokemon::StoryFailure;


std::vector<std::pair<std::string, BdspStep>> story_steps(){
    std::vector<std::pair<std::string, BdspStep>> out;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const BdspStep& step : row.steps){
            out.emplace_back(row.id, step);
        }
    }
    return out;
}

bool needs_console(const BdspStep& step){
    return step.action != "route" && step.action != "legs" && step.action != "step" &&
        step.action != "talk" && step.action != "scene";
}

BdspStoryHandlers naming_handlers(){
    const auto named = [](std::string tag){
        return [tag = std::move(tag)](const BdspStep&, BdspRunState&) -> std::optional<StoryFailure>{
            return StoryFailure{.code = "console", .detail = tag};
        };
    };
    return BdspStoryHandlers{
        .new_game = named("new_game"),
        .options = named("options"),
        .answer = named("answer"),
        .battle = named("battle"),
        .train = named("train"),
        .heal = named("heal"),
        .save = named("save"),
        .event = [](const std::string& name) -> BdspStepAction{
            return [name](const BdspStep&, BdspRunState&) -> std::optional<StoryFailure>{
                return StoryFailure{.code = "event", .detail = name};
            };
        },
    };
}

//  Every embedded S10/S11 plan parses directly through #648's
//  Pokemon::read_story_checkpoint() and every step has a known action.
UnitTestResult test_every_step_has_an_action(Logger& logger){
    size_t plans = 0;
    size_t steps = 0;
    Pokemon::StoryActions actions = make_bdsp_action_parsers([](BdspStep) -> Pokemon::StoryStepRun{
        return [](Pokemon::StoryBackend&, Pokemon::RunState&) -> std::optional<StoryFailure>{
            return std::nullopt;
        };
    });
    for (const auto& [name, text] : bdsp_checkpoint_json()){
        if (!name.ends_with(".plan")){
            continue;
        }
        const Pokemon::StoryCheckpoint cp = Pokemon::read_story_checkpoint(parse_json(text), "bdsp", name, actions);
        for (const Pokemon::StoryStep& step : cp.steps){
            if (!is_known_bdsp_action(step.action)){
                return std::format("{} {}: unknown action \"{}\".", cp.id, step.name, step.action);
            }
        }
        plans++;
        steps += cp.steps.size();
    }
    logger.log(std::format("{} plans, {} steps parsed directly through #648 read_story_checkpoint().", plans, steps));
    return true;
}

UnitTestResult test_dispatch_routes_every_action(Logger& logger){
    GridSim sim({}, {0, 0}, Pokemon::Direction::NORTH, false);
    std::vector<std::string> notes;
    SimMisses misses;
    const SimWorldKnobs knobs;
    const std::string step_name;
    SimStoryStepIO io(sim, notes, misses, knobs, step_name, []{ return false; }, [](const Pokemon::GridPose&){});
    StoryWalkSteps walks(io);
    const BdspStoryHandlers handlers = naming_handlers();

    std::set<std::string> seen;
    for (const auto& [row, step] : story_steps()){
        if (!seen.insert(step.action).second){
            continue;
        }
        if (!needs_console(step)){
            continue;
        }
        BdspRunState state;
        const std::optional<StoryFailure> failure = run_bdsp_action(walks, handlers, step, state);
        const std::string expected_code = story_events().contains(step.action) ? "event" : "console";
        if (!failure || failure->code != expected_code || failure->detail != step.action){
            return std::format("{} {} (\"{}\"): expected {}: {}, got {}.",
                row, step.name, step.action, expected_code, step.action,
                failure ? std::format("{}", *failure) : std::string("ok"));
        }
    }
    logger.log(std::format("{} unique BDSP actions dispatched cleanly.", seen.size()));
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
                    return std::format("{} {}: stubbed, but the simulation plays its action.", row, step.name);
                }
            }
        }
    }
    if (!sim.result().error.empty()){
        return "The run failed: " + sim.result().error;
    }
    return true;
}

UnitTestResult test_scene_start_waits(Logger& logger){
    constexpr std::string_view LAST_ROW = "01.11";
    constexpr size_t EXPECTED_SCENES = 23;
    constexpr std::string_view DEFAULT_SKIP = "barry_goes_home";
    SimExecutor sim(logger);
    run_rows_on(sim, logger, {.from = "00.00", .to = std::string(LAST_ROW)});
    if (!sim.result().error.empty()){
        return std::format("The run failed: {}", sim.result().error);
    }
    size_t scenes = 0;
    size_t guarded = 0;
    for (const auto& [row, step] : story_steps()){
        if (row > LAST_ROW || step.action != "scene"){
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

UnitTestResult test_intervening_step_keeps_start_wait(Logger& logger){
    const std::string_view intervening_actions[] = {"talk", "battle"};
    for (std::string_view action : intervening_actions){
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
        SimStoryStepIO io(grid, notes, misses, knobs, io_step, []{ return false; }, [](const Pokemon::GridPose&){});
        StoryWalkSteps walks(io);
        BdspStoryHandlers handlers = naming_handlers();
        handlers.battle = [](const BdspStep&, BdspRunState&) -> std::optional<StoryFailure>{ return std::nullopt; };
        BdspRunState state;

        BdspStep preceding{.name = "preceding_scene", .action = "scene", .args = SceneStepArgs{}};
        run_bdsp_action(walks, handlers, preceding, state);
        misses.dialog_waits.clear();
        BdspStep intervening{.name = "intervening_step", .action = std::string(action), .args = PlainStepArgs{}};
        if (action == "battle"){
            intervening.args = BattleStepArgs{};
        }
        run_bdsp_action(walks, handlers, intervening, state);
        misses.dialog_waits.clear();
        BdspStep following{.name = "following_scene", .action = "scene", .args = SceneStepArgs{}};
        run_bdsp_action(walks, handlers, following, state);

        const auto found = misses.dialog_waits.find(io_step);
        if (found == misses.dialog_waits.end() || found->second != Milliseconds(15000)){
            return std::format("A scene after {} did not keep its full opening-line guard.", action);
        }
    }
    logger.log("Scenes after talk and battle keep their full opening-line guard.");
    return true;
}


struct AutoStoryStoryActionsCase{
    struct Input{
        std::function<UnitTestResult(Logger&)> body;
    };
    struct Expectation{
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_AutoStoryStoryActions : public UnitTest{
public:
    explicit Test_AutoStoryStoryActions(AutoStoryStoryActionsCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.name)
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
    {.name = "Story actions: every step of every row parses through #648 read_story_checkpoint()", .input = {test_every_step_has_an_action}, .expectation = {}},
    {.name = "Story actions: every console and event action dispatches to its handler", .input = {test_dispatch_routes_every_action}, .expectation = {}},
    {.name = "Story actions: the steps the sim stubs from 00.00 to 01.11 all need the console", .input = {test_stubbed_steps_need_the_console}, .expectation = {}},
    {.name = "Story actions: scene starts retain the full guard until dialogue is observed", .input = {test_scene_start_waits}, .expectation = {}},
    {.name = "Story actions: a scene played by the preceding settle skips its start wait", .input = {test_seen_scene_skips_start_wait}, .expectation = {}},
    {.name = "Story actions: an intervening step invalidates a prior settle's dialogue", .input = {test_intervening_step_keeps_start_wait}, .expectation = {}},
};

void add_tests_AutoStory_StoryActions(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_ACTIONS_CASES){
        database.add<Test_AutoStoryStoryActions>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
