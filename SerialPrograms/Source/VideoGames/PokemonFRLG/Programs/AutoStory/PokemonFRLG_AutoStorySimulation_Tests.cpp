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
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryEvents.h"
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
    //  What the Mart sold, by its list's names.
    std::map<std::string, int> bag;

    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override{
        m_row = data.id;
        trace.push_back("begin " + data.id);
    }
    virtual Outcome run(RunState& state, const StoryStep& step) override{
        const std::string key = m_row + "/" + step.name;
        trace.push_back(key);
        auto failure = failures.find(key);
        if (failure != failures.end() && failure->second.times > 0){
            failure->second.times--;
            return Pokemon::outcome_failed(failure->second.failure, key);
        }
        return Pokemon::StoryStepExecutor::run(state, step);
    }
protected:
    virtual Outcome route(RunState& state, const StoryStep& step) override{
        return walk_to(state.pose, step);
    }
    virtual Outcome legs(RunState& state, const StoryStep& step) override{
        const Standing at = standing(state.pose);
        if (!at.error.empty()){
            return Pokemon::outcome_failed("no_pose", at.error);
        }
        const std::optional<std::vector<RouteStep>> route = fixed_route(story_maps(), at.position, step.legs);
        if (!route || route->empty()){
            return Pokemon::outcome_failed("bad_legs", to_string(at.position));
        }
        return Pokemon::outcome_ok(to_pose(StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after}));
    }
    //  A step into a trainer's line goes on until a trainer can see the
    //  player, as step_until_seen() does on the console.
    virtual Outcome step(RunState& state, const StoryStep& step) override{
        const std::optional<StoryPose> at = story_pose(state.pose);
        if (!at){
            return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
        }
        const MapData* map = map_named(at->map);
        if (step.variant != TRAINER_LINE_VARIANT || map == nullptr){
            return Pokemon::outcome_ok(to_pose(StoryPose{at->map, at->tile.next(step.direction), step.direction}));
        }
        TilePos onto = at->tile;
        const std::optional<int> stepped = step_until_seen(
            TRAINER_LINE_TILES,
            [&]{ onto = onto.next(step.direction); },
            [&]{
                return std::any_of(map->objects.begin(), map->objects.end(), [&](const MapObject& object){
                    return object.trainer && trainer_may_see(*map, object, onto);
                });
            }
        );
        if (!stepped){
            return Pokemon::outcome_failed("no_trainer", to_string(MapPosition{at->map, onto}));
        }
        return Pokemon::outcome_ok(to_pose(StoryPose{at->map, onto, step.direction}));
    }
    //  The nurse's way must plan too, but the run goes on from where it stands.
    virtual Outcome ready(RunState& state, const StoryStep& step) override{
        const Outcome to_nurse = walk_to(state.pose, step);
        return to_nurse.succeeded() ? Pokemon::outcome_ok() : to_nurse;
    }
    virtual Outcome new_game(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome options(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome talk(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome answer(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome mash(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome battle(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome heal(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    virtual Outcome game(RunState&, const StoryStep&) override{ return Pokemon::outcome_ok(); }
    //  The Viridian Mart's list as the game shows it. After a purchase the
    //  cursor stays on the row bought (console B, 2026-11-01), so each
    //  order moves it from there, as buy_items() does since ad5d40885.
    virtual Outcome shop(RunState&, const StoryStep& step) override{
        static const std::vector<std::string> LIST{"poke_ball", "potion", "antidote", "parlyz_heal", "cancel"};
        const std::optional<std::vector<MartOrder>> orders = viridian_mart_orders(step.items);
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
            if (LIST[cursor] != step.items[c].item){
                return Pokemon::outcome_failed("wrong_item", std::format("{} instead of {}", LIST[cursor], step.items[c].item));
            }
            bag[LIST[cursor]] += order.quantity;
        }
        return Pokemon::outcome_ok();
    }

private:
    Outcome walk_to(const Pokemon::Pose& pose, const StoryStep& step){
        const Standing at = standing(pose);
        if (!at.error.empty()){
            return Pokemon::outcome_failed("no_pose", at.error);
        }
        const std::optional<StoryPose> target = story_pose(*step.target);
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

    //  A language no reader supports, so a step that needed text would show.
    Pokemon::SegmentReport run(const Pokemon::SegmentRange& range){
        game.set_language(Language::Japanese);
        Pokemon::Segment segment;
        segment.id = "00";
        for (const StoryCheckpoint& data : rows){
            segment.checkpoints.push_back(Pokemon::CheckpointSpec{
                .id = data.id, .title = data.title, .saves = true,
                .build = [this, &data]{ return Pokemon::build_story_checkpoint(data, game); },
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
        return Pokemon::run_segment(state, segment, range, policy, hooks);
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


class Test_AutoStorySimulation : public UnitTest{
public:
    Test_AutoStorySimulation(std::string name, std::function<UnitTestResult()> body)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + std::move(name))
        , m_body(std::move(body))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_body();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    std::function<UnitTestResult()> m_body;
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
        const std::optional<StoryPose> start = story_pose(*data.start);
        const std::optional<StoryPose> saved = story_pose(sim.saves[c - 1].second);
        const bool same = start && saved && start->map == saved->map && start->tile == saved->tile
            && (!start->facing || start->facing == saved->facing);
        if (!same){
            return std::format("{} starts at {}, but {} saved at {}.", data.id,
                Pokemon::to_string(*data.start), sim.saves[c - 1].first, Pokemon::to_string(sim.saves[c - 1].second));
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

//  The route the program walks for `step`, a ROUTE or LEGS, from `at`:
//  planned, or the legs laid out.
std::optional<std::vector<RouteStep>> walked_route(const StoryPose& at, const StoryStep& step){
    const MapPosition start{at.map, at.tile};
    if (step.kind == StoryStepKind::LEGS){
        return fixed_route(story_maps(), start, step.legs);
    }
    const std::optional<StoryPose> target = story_pose(*step.target);
    const RouteGoal goal{{target->map, target->tile}, target->facing};
    return plan_overworld_route(story_maps(), start, goal, default_route_costs(), at.facing);
}

//  Walks `step` of 00.14 on `sim` from `at`, where the table says the
//  player stands, and moves `at` to where it says the step ends. Returns
//  the failure, or empty.
std::string walk_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step){
    const std::string where = "00.14/" + step.name + ": ";
    //  A walk that ends on an open leg may leave the player short of it.
    bool open_end = false;
    if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step);
        if (!route || route->empty()){
            return where + "no route from " + to_string(MapPosition{at.map, at.tile}) + ".";
        }
        const RoutePlanning planning{.maps = &story_maps(), .start = MapPosition{at.map, at.tile}, .label = step.name};
        const std::string failure = OverworldSimWalker(sim).run(*route, planning);
        if (!failure.empty()){
            return where + failure + " On " + to_string(sim.state().position) + ".";
        }
        const RouteStep& last = route->back();
        open_end = last.action == RouteAction::WALK && !last.to_wall && !last.warps;
        at = StoryPose{last.end.map, last.end.point, last.facing_after};
    }else if (step.kind == StoryStepKind::STEP){
        if (sim.state().facing != step.direction){
            sim.tap(step.direction);
        }
        sim.tap(step.direction);
        sim.watch({SimWatcher::MESSAGE}, std::chrono::milliseconds(2000));
        at = *story_pose(*step.pose_after);
    }
    if (!open_end && !(sim.state().position == MapPosition{at.map, at.tile})){
        return where + "expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
    }
    return "";
}

//  Console B, 2026-11-03 (row 00.14): the open leg east from Route 4
//  into Cerulean City ended a tile short, on (6, 20), with nothing on
//  screen. No hold to a wall finds the player there, and the walk went on
//  a column off, the hold south stopped by the fence on (6, 23), to a
//  wall below a row of houses, not the Pokemon Center's door. From the
//  foot of Route 4's ledge, the row's walks on the sim reach the nurse
//  with that leg whole or a tile short.
UnitTestResult test_cerulean_entry_short(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "00.14"; });
    if (row == rows.end()){
        return "No checkpoint 00.14.";
    }
    const auto hop = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "ledge_hop"; });
    if (hop == row->steps.end() || !hop->pose_after){
        return "00.14: no ledge_hop.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(*hop->pose_after);
        SimScript script;
        script.short_holds[1] = short_by;
        OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
        for (auto step = hop + 1; step != row->steps.end() && step->kind != StoryStepKind::HEAL; ++step){
            const std::string failure = walk_on_sim(sim, at, *step);
            if (!failure.empty()){
                return std::format("Entry {} tile(s) short: ", short_by) + failure;
            }
        }
    }
    return true;
}

//  Runs `step` of row `id` on `sim` as the program does: routes and legs
//  by the sim's walker from `at`, where the table says the player stands,
//  fighting a trainer who sees the player on the way; a step one tap at a
//  time, or for a step into a trainer's line by step_until_seen(), each
//  watched for the trainer; a battle from the trainer's message, which
//  must be up. Moves `at` to where the table says the step ends. Returns
//  the failure, or empty.
std::string row_step_on_sim(OverworldSim& sim, StoryPose& at, const StoryStep& step, const std::string& id){
    const std::string where = id + "/" + step.name + ": ";
    const auto on = [&sim]{ return " On " + to_string(sim.state().position) + "."; };
    if (step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::ROUTE){
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step);
        const RoutePlanning planning{.maps = &story_maps(), .start = MapPosition{at.map, at.tile}, .label = step.name};
        const std::string failure = route ? OverworldSimWalker(sim).run(*route, planning) : "no legs.";
        at = route ? StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after} : at;
        return failure.empty() ? "" : where + failure + on();
    }
    if (step.kind == StoryStepKind::STEP){
        const auto tap = [&sim, &step]{
            if (sim.state().facing != step.direction){
                sim.tap(step.direction);
            }
            sim.tap(step.direction);
        };
        const auto seen = [&sim]{
            return sim.watch({SimWatcher::MESSAGE, SimWatcher::BATTLE_SCREEN}, TRAINER_LINE_WATCH).has_value();
        };
        const int tiles = step.variant == TRAINER_LINE_VARIANT ? TRAINER_LINE_TILES : 1;
        return step_until_seen(tiles, tap, seen) ? "" : where + "no trainer saw the player." + on();
    }
    if (step.kind == StoryStepKind::BATTLE){
        if (!sim.close_message() || sim.handle_battle(SimBattleChoice::FIGHT) != SimBattleEnd::ENDED){
            return where + "no_battle." + on();
        }
        at = *story_pose(*step.pose_after);
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return where + "expected " + to_string(MapPosition{at.map, at.tile}) + "." + on();
        }
    }
    return "";
}

//  Console B, 2026-11-03 (row 00.16), twice: the open leg north from
//  Cerulean City onto Nugget Bridge ended on (11, 33), a tile short of
//  (11, 32), and the one step onto Cale's row reached only (11, 32), out
//  of his sight: "bug_catcher_cale: failed: no_battle". From the walk's
//  end, whole or a tile short, the five trainers are met and beaten on
//  the sim, each battle on the trainer's row.
UnitTestResult test_nugget_bridge_short(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "00.16"; });
    if (row == rows.end()){
        return "No checkpoint 00.16.";
    }
    const auto walk = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "below_cale_line"; });
    if (walk == row->steps.end() || !walk->target){
        return "No step 00.16/below_cale_line.";
    }
    for (int short_by : {0, 1}){
        StoryPose at = *story_pose(*walk->target);
        OverworldSim sim(story_maps(), MapPosition{at.map, {at.tile.x, at.tile.y + short_by}}, Direction::NORTH);
        for (auto step = walk + 1; step != row->steps.end() && step->kind != StoryStepKind::READY; ++step){
            const std::string failure = row_step_on_sim(sim, at, *step, "00.16");
            if (!failure.empty()){
                return std::format("Walk {} tile(s) short: ", short_by) + failure;
            }
        }
        if (sim.state().trainers_beaten.size() != 5){
            return std::format("Walk {} tile(s) short: expected 5 trainers beaten, got {}.", short_by, sim.state().trainers_beaten.size());
        }
    }
    return true;
}

//  00.17 on the sim, from its start on Route 24 to the Sea Cottage:
//  Franklin, Kelsey, Nob and Haley are met on the steps into their lines
//  and beaten there. The sim's trainers who look around see every way
//  they may look, so Youngster Dan sees the player on (20, 4), walks up
//  to (21, 4) and is fought there, off the way on.
UnitTestResult test_route25_trainers(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "00.17"; });
    if (row == rows.end()){
        return "No checkpoint 00.17.";
    }
    StoryPose at = *story_pose(*row->start);
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row->steps){
        const std::string failure = row_step_on_sim(sim, at, step, "00.17");
        if (!failure.empty()){
            return failure;
        }
    }
    const std::vector<std::string> expected{"hiker", "youngster", "picnicker", "hiker", "lass"};
    if (sim.state().trainers_beaten != expected){
        std::string beaten;
        for (const std::string& name : sim.state().trainers_beaten){
            beaten += " " + name;
        }
        return "00.17: expected Franklin, Dan, Kelsey, Nob and Haley beaten; beat" + beaten + ".";
    }
    if (!(sim.state().position == MapPosition{"Route25_SeaCottage", {7, 8}})){
        return "00.17: expected to end in the Sea Cottage on (7, 8), not " + to_string(sim.state().position) + ".";
    }
    return true;
}

//  00.20 on the sim, its walks from the Center: over Nugget Bridge, whose
//  trainers 00.16 beat (they stand there, but don't battle), onto Route
//  24's grass at (4, 19) out of Camper Shane's sight, and back to the
//  nurse. No trainer battles on the way.
UnitTestResult test_oddish_walks(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "00.20"; });
    if (row == rows.end()){
        return "No checkpoint 00.20.";
    }
    std::vector<MapData> maps = story_maps();
    for (MapData& map : maps){
        if (map.name != "Route24"){
            continue;
        }
        for (MapObject& object : map.objects){
            if (object.trainer && object.x >= 10 && object.x <= 12){
                object.trainer.reset();
            }
        }
    }
    StoryPose at = *story_pose(*row->start);
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing);
    const std::map<std::string, MapPosition> ends{
        {"mart_clerk", {"CeruleanCity_Mart", {4, 3}}},
        {"oddish_patch", {"Route24", {4, 19}}},
        {"cerulean_nurse", {"CeruleanCity_PokemonCenter_1F", {7, 4}}},
    };
    for (const StoryStep& step : row->steps){
        if (step.kind != StoryStepKind::ROUTE){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, step, "00.20");
        if (!failure.empty()){
            return failure;
        }
        const auto end = ends.find(step.name);
        if (end == ends.end() || !(sim.state().position == end->second)){
            return "00.20/" + step.name + ": ended on " + to_string(sim.state().position) + ".";
        }
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "00.20: a trainer's battle or a violation on the way.";
    }
    return true;
}

//  00.14 on the sim, from Mt. Moon's exit (32, 6): east and up through
//  (48, 5) into the tutors' pen on Route 4 to (50, 4), facing the Mega
//  Kick tutor (50, 3), then on to above the ledge (93, 8). Each walk ends
//  where the table says, and no trainer sees the player on the way.
UnitTestResult test_mega_kick_tutor_walks(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "00.14"; });
    if (row == rows.end()){
        return "No checkpoint 00.14.";
    }
    const auto ledge = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "above_ledge"; });
    if (ledge == row->steps.end()){
        return "00.14: no above_ledge.";
    }
    StoryPose at = *story_pose(*row->start);
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    bool at_tutor = false;
    for (auto step = row->steps.begin(); step != ledge + 1; ++step){
        if (step->kind != StoryStepKind::ROUTE && step->kind != StoryStepKind::LEGS){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, *step, "00.14");
        if (!failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "00.14/" + step->name + ": expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
        }
        if (sim.state().position == MapPosition{"Route4", {50, 4}} && sim.state().facing == Direction::NORTH){
            at_tutor = true;
        }
    }
    if (!at_tutor){
        return "00.14: no walk ended on Route 4's (50, 4) facing the Mega Kick tutor (50, 3).";
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "00.14: a trainer's battle or a violation on the way.";
    }
    return true;
}

//  Console B, 2026-11-03 (row 00.18): after his scene Bill stands on
//  (7, 6), east of the player on (6, 6), not on the maps' (7, 5). The
//  walk out planned east onto (7, 6) and south to the mat (7, 9); the
//  east tap bumped Bill, and the hold south took the player down column 6
//  to (6, 9), beside the mat, with no warp. On a copy of the maps with
//  Bill on (7, 6) and only the mat (7, 9) as the way out (the console
//  didn't warp from (6, 9)), the row's walks after the ticket end each
//  where the table says and leave on Route 25's (51, 5).
UnitTestResult test_out_of_bill_cottage(){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [](const StoryCheckpoint& data){ return data.id == "00.18"; });
    if (row == rows.end()){
        return "No checkpoint 00.18.";
    }
    const auto ticket = std::find_if(row->steps.begin(), row->steps.end(), [](const StoryStep& step){ return step.name == "ss_ticket"; });
    if (ticket == row->steps.end() || !ticket->pose_after){
        return "00.18: no ss_ticket.";
    }
    std::vector<MapData> maps = story_maps();
    for (MapData& map : maps){
        if (map.name != "Route25_SeaCottage"){
            continue;
        }
        for (MapObject& object : map.objects){
            if (object.name == "bill"){
                object.x = 7;
                object.y = 6;
            }
        }
        std::erase_if(map.warps, [](const MapWarp& warp){ return warp.x != 7; });
    }
    StoryPose at = *story_pose(*ticket->pose_after);
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing);
    for (auto step = ticket + 1; step != row->steps.end(); ++step){
        if (step->kind != StoryStepKind::ROUTE && step->kind != StoryStepKind::LEGS){
            continue;
        }
        const std::string failure = row_step_on_sim(sim, at, *step, "00.18");
        if (!failure.empty()){
            return failure;
        }
        if (!(sim.state().position == MapPosition{at.map, at.tile})){
            return "00.18/" + step->name + ": expected " + to_string(MapPosition{at.map, at.tile}) + ", but on " + to_string(sim.state().position) + ".";
        }
    }
    if (!(sim.state().position == MapPosition{"Route25", {51, 5}})){
        return "00.18: expected to leave onto Route 25's (51, 5), not " + to_string(sim.state().position) + ".";
    }
    if (!sim.state().violations.empty()){
        return "00.18: violations:" + joined(sim.state().violations);
    }
    return true;
}

}


void add_tests_AutoStorySimulation(UnitTestDatabase& database){
    const auto add = [&](std::string name, UnitTestResult (*body)()){
        database.add<Test_AutoStorySimulation>(std::move(name), body);
    };
    add("new game to Brock, in order, each start where the last save stood", test_whole_story);
    add("start and end checkpoints", test_start_and_end);
    add("a failed walk reloads its checkpoint's save", test_retry_from_save);
    add("a battle lost every time stops after 3 attempts", test_gives_up_after_attempts);
    add("00.14 into Cerulean, the entry leg a tile short", test_cerulean_entry_short);
    add("00.14 from Mt. Moon to the Mega Kick tutor, then above the ledge", test_mega_kick_tutor_walks);
    add("00.16 Nugget Bridge, the walk onto it a tile short", test_nugget_bridge_short);
    add("00.17 Route 25, its four trainers and Dan", test_route25_trainers);
    add("00.18 out of Bill's Sea Cottage, Bill on (7, 6)", test_out_of_bill_cottage);
    add("00.20 to Route 24's grass and back, past Shane", test_oddish_walks);
}



}
}
}
