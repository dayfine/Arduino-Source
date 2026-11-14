/*  AutoStory Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of the whole story table, new game to the Boulder Badge, without
 *  the console: story_checkpoints() built into engine checkpoints by the
 *  shared builder, saving at each end as the program does, run by the
 *  shared runner with the program's retry policy, against a fake game.
 *  The fake plans each ROUTE on the story maps from where the run's pose
 *  says we stand (turning to a wall when the facing is unknown, as the
 *  program does), walks each LEGS on them, steps onto triggers, and passes
 *  every other step unless told to fail it.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using Pokemon::Outcome;
using Pokemon::RunState;

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n    " + line;
    }
    return text;
}

//  A checkpoint's start as the engine's Pose: nothing for a new game.
std::optional<Pokemon::Pose> start_pose(const StoryCheckpoint& checkpoint){
    if (!checkpoint.start){
        return std::nullopt;
    }
    return Pokemon::pose_of(*checkpoint.start);
}

const MapData* map_named(const std::string& name){
    for (const MapData& map : story_maps()){
        if (map.name == name){
            return &map;
        }
    }
    return nullptr;
}

//  Whether the player, at the end of a WALK `step`, stops there holding on:
//  the map's edge, a wall, or water to Surf onto, but no warp of any kind,
//  even one to a map that isn't loaded.
bool stops_past_end(const RouteStep& step){
    const MapData& map = *map_named(step.end.map);
    const TilePos here = step.end.point;
    const TilePos ahead = here.next(step.direction);
    if (!in_bounds(map, ahead)){
        return true;
    }
    if (warps_on_step(map, ahead) || exit_mat(map, here) == step.direction
        || (step.direction == Direction::NORTH && is_door(map, ahead))
    ){
        return false;
    }
    const StepKind kind = classify_step(map, PlayerState{here, elevation_at(map, here), step.surfing}, step.direction);
    return kind == StepKind::BLOCKED || kind == StepKind::START_SURF;
}

//  The first leg of `route` to a wall that holding on would carry past its
//  end, or nothing (console B, 2026-11-02: one rode Route 4's Pokemon
//  Center escalator).
std::optional<std::string> overrun_wall_leg(const std::vector<RouteStep>& route){
    for (const RouteStep& step : route){
        if (step.to_wall && !stops_past_end(step)){
            return to_string(step);
        }
    }
    return std::nullopt;
}

//  Where the run stands, with a facing: a wall beside it when unknown.
struct Standing{
    MapPosition position;
    Direction facing = Direction::NORTH;
    std::string error;
};
Standing standing(const Pokemon::Pose& pose){
    const std::optional<StoryPose> at = story_pose(pose);
    if (!at){
        return {{}, Direction::NORTH, "no one-tile pose: " + Pokemon::to_string(pose)};
    }
    std::optional<Direction> facing = at->facing;
    if (!facing){
        const MapData* map = map_named(at->map);
        facing = map ? wall_beside(*map, at->tile) : std::nullopt;
    }
    if (!facing){
        return {{}, Direction::NORTH, "facing unknown and no wall beside " + to_string(MapPosition{at->map, at->tile})};
    }
    return {{at->map, at->tile}, *facing, ""};
}

class FakeGame : public Pokemon::StoryStepExecutor{
public:
    struct Failure{
        std::string failure;
        size_t times = 0;
    };

    std::vector<std::string> trace;
    std::map<std::string, Failure> failures;
    //  Where the save loaded stands, when not at its checkpoint's start.
    std::optional<StoryPose> save_stands;
    //  What the Mart sold, by its list's names.
    std::map<std::string, int> bag;

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        trace.push_back("begin " + data.id);
    }
    virtual Outcome run(const StoryStep& step, RunState& state) override{
        const std::string key = m_row + "/" + step.name;
        trace.push_back(key);
        auto failure = failures.find(key);
        if (failure != failures.end() && failure->second.times > 0){
            failure->second.times--;
            return Pokemon::outcome_failed(failure->second.failure, key);
        }
        return act(state, step);
    }
private:
    //  The step by its kind, as the console does it (StoryExecutor), with
    //  nothing done on a screen.
    Outcome act(RunState& state, const StoryStep& step){
        switch (step.kind){
        case StoryStepKind::ROUTE:
            return walk_to(state.pose, step);
        case StoryStepKind::LEGS:
            return legs(state, step);
        case StoryStepKind::STEP:
            return take_step(state, step);
        case StoryStepKind::READY:
            return ready(state, step);
        case StoryStepKind::SHOP:
            return shop(step);
        case StoryStepKind::NEW_GAME:
        case StoryStepKind::OPTIONS:
        case StoryStepKind::TALK:
        case StoryStepKind::ANSWER:
        case StoryStepKind::MASH:
        case StoryStepKind::BATTLE:
        case StoryStepKind::HEAL:
        case StoryStepKind::GAME:
            return Pokemon::outcome_ok();
        default:
            return Pokemon::unsupported_step(step);
        }
    }
    Outcome legs(RunState& state, const StoryStep& step){
        const Standing at = standing(state.pose);
        if (!at.error.empty()){
            return Pokemon::outcome_failed("no_pose", at.error);
        }
        const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), at.position, Pokemon::story_arg<std::vector<StoryLeg>>(step.args, "legs"));
        if (!route || route->empty()){
            return Pokemon::outcome_failed("bad_legs", to_string(at.position));
        }
        return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
    }
    Outcome take_step(RunState& state, const StoryStep& step){
        const std::optional<StoryPose> at = story_pose(state.pose);
        if (!at){
            return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
        }
        const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
        return Pokemon::outcome_ok(to_pose(StoryPose{at->map, at->tile.next(direction), direction}));
    }
    //  The nurse's way must plan too, but the run goes on from where it stands.
    Outcome ready(RunState& state, const StoryStep& step){
        const Outcome to_nurse = walk_to(state.pose, step);
        return to_nurse.succeeded() ? Pokemon::outcome_ok() : to_nurse;
    }
    //  The Viridian Mart's list as the game shows it. After a purchase the
    //  cursor stays on the row bought (console B, 2026-11-01), so each
    //  order moves it from there, as buy_items() does since ad5d40885.
    Outcome shop(const StoryStep& step){
        const std::vector<Pokemon::StoryItem>& items = Pokemon::story_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
        static const std::vector<std::string> LIST{"poke_ball", "potion", "antidote", "parlyz_heal", "cancel"};
        const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(items);
        if (!orders){
            return Pokemon::outcome_failed("unknown_item", step.name);
        }
        int cursor = 0;
        for (size_t c = 0; c < orders->size(); c++){
            const MartOrder& order = (*orders)[c];
            //  The presses: down or up from where the cursor is.
            cursor += order.row - cursor;
            if (cursor < 0 || cursor >= (int)LIST.size() || LIST[cursor] == "cancel"){
                return Pokemon::outcome_failed("mart_cancel", std::format("order {} ends on row {}", c + 1, cursor + 1));
            }
            if (LIST[cursor] != items[c].item){
                return Pokemon::outcome_failed("wrong_item", std::format("{} instead of {}", LIST[cursor], items[c].item));
            }
            bag[LIST[cursor]] += order.quantity;
        }
        return Pokemon::outcome_ok();
    }

    Outcome walk_to(const Pokemon::Pose& pose, const StoryStep& step){
        const Standing at = standing(pose);
        if (!at.error.empty()){
            return Pokemon::outcome_failed("no_pose", at.error);
        }
        const std::optional<StoryPose> target = story_pose(Pokemon::pose_of(Pokemon::story_arg<Pokemon::GridPose>(step.args, "target")));
        const RouteGoal goal{{target->map, target->tile}, target->facing};
        const std::optional<std::vector<RouteStep>> route = plan_overworld_route(
            story_maps(), at.position, goal, default_route_costs(), at.facing
        );
        if (!route){
            return Pokemon::outcome_failed("no_route", to_string(at.position) + " to " + to_string(goal.position));
        }
        const MapPosition end = follow_overworld_route(story_maps(), at.position, *route);
        if (!(end == goal.position)){
            return Pokemon::outcome_failed("wrong_end", to_string(end));
        }
        if (const std::optional<std::string> leg = overrun_wall_leg(*route)){
            return Pokemon::outcome_failed("wall_overrun", *leg);
        }
        const Direction facing = route->empty() ? at.facing : route->back().facing_after;
        return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, facing}));
    }

    std::string m_row;
};


//  One simulated run, segment built as the program builds it: the rows a
//  run takes with every handoff off, each saving at its end (hooks.save
//  stands for save_game()).
struct Simulation{
    std::vector<StoryCheckpoint> rows = plan_story_run(story_checkpoints(), 0, story_checkpoints().size() - 1, StoryHandoffs{}).rows;
    FakeGame game;
    std::vector<std::string> lines;
    //  Where each save stood: the end of its checkpoint.
    std::vector<std::pair<std::string, Pokemon::Pose>> saves;
    Pokemon::SegmentPolicy policy;

    Pokemon::SegmentReport run(const Pokemon::SegmentRange& range){
        Pokemon::Segment segment;
        segment.id = "00";
        for (const StoryCheckpoint& data : rows){
            segment.checkpoints.push_back(Pokemon::CheckpointSpec{
                .id = data.id, .title = data.title, .saves = true,
                .build = [this, &data]{
                    Pokemon::Checkpoint checkpoint = Pokemon::build_story_checkpoint(data, game);
                    add_start_pose_check(checkpoint, start_pose(data), [this]{ return game.save_stands; }, [this](const std::string& line){ lines.push_back(line); });
                    return checkpoint;
                },
            });
        }
        Pokemon::SegmentHooks hooks;
        hooks.save = [this](RunState& state){
            game.trace.push_back("save");
            saves.emplace_back(last_begun(), state.pose);
            return Pokemon::outcome_ok();
        };
        hooks.reload = [this](RunState&){
            game.trace.push_back("reload");
            return Pokemon::outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return Pokemon::run_segment(range, policy, segment, hooks, state);
    }
    std::string last_begun() const{
        for (auto it = game.trace.rbegin(); it != game.trace.rend(); ++it){
            if (it->starts_with("begin ")){
                return it->substr(6);
            }
        }
        return "";
    }
    std::vector<std::string> begun() const{
        std::vector<std::string> ids;
        for (const std::string& item : game.trace){
            if (item.starts_with("begin ")){
                ids.push_back(item.substr(6));
            }
        }
        return ids;
    }
    size_t count(const std::string& item) const{
        return (size_t)std::count(game.trace.begin(), game.trace.end(), item);
    }
};


struct AutoStorySimulationInput{
    std::string name;
    UnitTestResult (*body)();
};

struct AutoStorySimulationExpectation{
    std::string_view description;
};

struct AutoStorySimulationTestCase{
    AutoStorySimulationInput input;
    AutoStorySimulationExpectation expectation;
};

class Test_AutoStorySimulation : public UnitTest{
public:
    explicit Test_AutoStorySimulation(AutoStorySimulationTestCase scenario)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_scenario.input.body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    AutoStorySimulationTestCase m_scenario;
};


UnitTestResult test_whole_story(){
    Simulation sim;
    const Pokemon::SegmentReport report = sim.run({});
    if (!report.completed){
        return "Expected the whole story through:" + joined(sim.lines);
    }
    std::vector<std::string> ids;
    for (const StoryCheckpoint& data : sim.rows){
        ids.push_back(data.id);
    }
    if (sim.begun() != ids || sim.saves.size() != ids.size()){
        return "Expected each checkpoint once, in order, each saving at its end:" + joined(sim.game.trace);
    }
    //  Each checkpoint starts where the save before it stands.
    for (size_t c = 1; c < sim.rows.size(); c++){
        const StoryCheckpoint& data = sim.rows[c];
        const std::optional<StoryPose> start = story_pose(*start_pose(data));
        const std::optional<StoryPose> saved = story_pose(sim.saves[c - 1].second);
        const bool same = start && saved && start->map == saved->map && start->tile == saved->tile
            && (!start->facing || start->facing == saved->facing);
        if (!same){
            return std::format("{} starts at {}, but {} saved at {}.", data.id,
                Pokemon::to_string(*start_pose(data)), sim.saves[c - 1].first, Pokemon::to_string(sim.saves[c - 1].second));
        }
    }
    if (report.one_shot_passes() != report.checkpoints_run){
        return "Expected every checkpoint one-shot:" + joined(sim.lines);
    }
    //  00.07's shopping, with the cursor staying on each row bought.
    const std::map<std::string, int> bought{{"potion", 5}, {"antidote", 3}, {"parlyz_heal", 1}};
    if (sim.game.bag != bought){
        std::string text;
        for (const auto& item : sim.game.bag){
            text += std::format(" {} {}", item.second, item.first);
        }
        return "Expected 5 Potions, 3 Antidotes and 1 Parlyz Heal; bought" + text + ".";
    }
    return true;
}

UnitTestResult test_start_and_end(){
    Simulation sim;
    const Pokemon::SegmentReport report = sim.run({.from = "00.05", .to = "00.07"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"00.05", "00.06", "00.07"}){
        return "Expected 00.05 to 00.07 only:" + joined(sim.begun());
    }
    return true;
}

//  A walk that fails once (a trainer the walker could not recover from):
//  the checkpoint reloads its last save and runs again from its start.
UnitTestResult test_retry_from_save(){
    Simulation sim;
    sim.game.failures["00.08/forest_north"] = {.failure = "operation_failed", .times = 1};
    const Pokemon::SegmentReport report = sim.run({.from = "00.08", .to = "00.09"});
    if (!report.completed || report.reloads != 1 || sim.begun() != std::vector<std::string>{"00.08", "00.08", "00.09"}
        || sim.count("00.08/forest_east") != 2
    ){
        return "Expected 00.08 reloaded once and run again from its start:" + joined(sim.game.trace);
    }
    if (std::find(sim.lines.begin(), sim.lines.end(), "[Checkpoint] 00.08: passed on attempt 2 (1 reset)") == sim.lines.end()){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

//  console B, FRLG 00.27, 2026-11-09: the save stood below Cerulean Center's
//  door, and the run walked as if from the counter where 00.27 starts. 00.09
//  from below Pewter Center's door (17, 25) stops before any step or save.
UnitTestResult test_save_off_start_pose(){
    Simulation sim;
    sim.game.save_stands = StoryPose{"PewterCity", TilePos{17, 26}, Direction::SOUTH};
    const Pokemon::SegmentReport report = sim.run({.from = "00.09", .to = "00.09"});
    const auto stepped = [](const std::string& item){ return item.starts_with("00.09/"); };
    const auto told = [](const std::string& line){ return line.starts_with("start pose mismatch: "); };
    if (report.completed || report.failure.failure != "start_failed" || !sim.saves.empty()
        || std::ranges::any_of(sim.game.trace, stepped) || std::ranges::none_of(sim.lines, told)
    ){
        return "Expected 00.09 stopped at its start check, the mismatch logged:" + joined(sim.game.trace) + joined(sim.lines);
    }
    return true;
}

//  Brock's battle lost every time: three attempts, then the stop.
UnitTestResult test_gives_up_after_attempts(){
    Simulation sim;
    sim.game.failures["00.09/brock"] = {.failure = "blackout", .times = 10};
    const Pokemon::SegmentReport report = sim.run({.from = "00.09", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "00.09" || report.failed_event != "brock"
        || report.failure.failure != "blackout" || sim.count("00.09/brock") != 3 || !sim.saves.empty()
    ){
        return "Expected 00.09 tried 3 times, then a stop on Brock:" + joined(sim.game.trace) + joined(sim.lines);
    }
    return true;
}

const AutoStorySimulationTestCase AUTO_STORY_SIMULATION_CASES[] = {
    {
        .input = {.name = "new game to Brock, in order, each start where the last save stood", .body = test_whole_story},
        .expectation = {.description = "runs full story progression from new game through Brock gym"},
    },
    {
        .input = {.name = "start and end checkpoints", .body = test_start_and_end},
        .expectation = {.description = "executes segment bounded by start and end checkpoints"},
    },
    {
        .input = {.name = "a failed walk reloads its checkpoint's save", .body = test_retry_from_save},
        .expectation = {.description = "reloads checkpoint save state on simulated movement failure"},
    },
    {
        .input = {.name = "a battle lost every time stops after 3 attempts", .body = test_gives_up_after_attempts},
        .expectation = {.description = "aborts after three consecutive battle blackouts"},
    },
    {
        .input = {.name = "a save off the start pose stops before any step", .body = test_save_off_start_pose},
        .expectation = {.description = "fails the start check without walking or saving"},
    },
};

} // namespace


void add_tests_AutoStorySimulation(UnitTestDatabase& database){
    add_tests_AutoStoryStepTable(database);
    add_tests_AutoStoryStartFrames(database);
    for (const auto& scenario : AUTO_STORY_SIMULATION_CASES){
        database.add<Test_AutoStorySimulation>(scenario);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
