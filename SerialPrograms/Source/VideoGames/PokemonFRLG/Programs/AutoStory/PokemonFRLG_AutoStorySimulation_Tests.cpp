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
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "Pokemon/Navigation/Pokemon_StorySteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
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
    virtual Outcome step(RunState& state, const StoryStep& step) override{
        const std::optional<StoryPose> at = story_pose(state.pose);
        if (!at){
            return Pokemon::outcome_failed("no_pose", Pokemon::to_string(state.pose));
        }
        return Pokemon::outcome_ok(to_pose(StoryPose{at->map, at->tile.next(step.direction), step.direction}));
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
        const Direction facing = route->empty() ? at.facing : route->back().facing_after;
        return Pokemon::outcome_ok(to_pose(StoryPose{target->map, target->tile, facing}));
    }

    std::string m_row;
};


//  One simulated run, segment built as the program builds it: each
//  checkpoint saves at its end (hooks.save stands for save_game()).
struct Simulation{
    FakeGame game;
    std::vector<std::string> lines;
    //  Where each save stood: the end of its checkpoint.
    std::vector<std::pair<std::string, Pokemon::Pose>> saves;
    Pokemon::SegmentPolicy policy;

    Pokemon::SegmentReport run(const Pokemon::SegmentRange& range){
        Pokemon::Segment segment;
        segment.id = "00";
        for (const StoryCheckpoint& data : story_checkpoints()){
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
    for (const StoryCheckpoint& data : story_checkpoints()){
        ids.push_back(data.id);
    }
    if (sim.begun() != ids || sim.saves.size() != ids.size()){
        return "Expected each checkpoint once, in order, each saving at its end:" + joined(sim.game.trace);
    }
    //  Each checkpoint starts where the save before it stands.
    for (size_t c = 1; c < story_checkpoints().size(); c++){
        const StoryCheckpoint& data = story_checkpoints()[c];
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
    sim.game.failures["00.07/forest_north"] = {.failure = "operation_failed", .times = 1};
    const Pokemon::SegmentReport report = sim.run({.from = "00.07", .to = "00.08"});
    if (!report.completed || report.reloads != 1 || sim.begun() != std::vector<std::string>{"00.07", "00.07", "00.08"}
        || sim.count("00.07/mart_clerk") != 2
    ){
        return "Expected 00.07 reloaded once and run again from its start:" + joined(sim.game.trace);
    }
    if (std::find(sim.lines.begin(), sim.lines.end(), "[Checkpoint] 00.07: passed on attempt 2 (1 reset)") == sim.lines.end()){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

//  Brock's battle lost every time: three attempts, then the stop.
UnitTestResult test_gives_up_after_attempts(){
    Simulation sim;
    sim.game.failures["00.08/brock"] = {.failure = "blackout", .times = 10};
    const Pokemon::SegmentReport report = sim.run({.from = "00.08", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "00.08" || report.failed_event != "brock"
        || report.failure.failure != "blackout" || sim.count("00.08/brock") != 3 || !sim.saves.empty()
    ){
        return "Expected 00.08 tried 3 times, then a stop on Brock:" + joined(sim.game.trace) + joined(sim.lines);
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
}



}
}
}
