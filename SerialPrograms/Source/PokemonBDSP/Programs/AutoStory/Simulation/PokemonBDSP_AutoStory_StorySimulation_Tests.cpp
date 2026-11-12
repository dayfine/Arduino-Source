/*  BDSP AutoStory Story Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of the whole story table, new game to Floaroma, without the
 *  console: story_checkpoints() built into one segment as the program builds
 *  it (make_story_segment()), run by the shared runner with the program's
 *  retry policy, against a fake game. The fake plans each ROUTE on the story
 *  world that holds its start and target, walks each LEGS tile by tile, and
 *  passes every other step unless told to fail it. Events pass as a whole.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_PlanGolden.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryWalkSteps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_StorySimulation_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::Outcome;
using Pokemon::Pose;
using Pokemon::RunState;
using Pokemon::StoryLeg;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;
using Pokemon::story_arg;

std::string describe(const std::string& map, const std::vector<TilePos>& tiles){
    std::string text = " " + map;
    for (const TilePos& pos : tiles){
        text += " " + Pokemon::to_string(pos);
    }
    return text;
}

std::string joined(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += "\n    " + line;
    }
    return text;
}

bool contains(const std::vector<TilePos>& tiles, const TilePos& pos){
    return std::find(tiles.begin(), tiles.end(), pos) != tiles.end();
}

//  Where a step ends, or why it does not plan. A "measured" step ends on
//  one of `at`'s tiles and the program knows which (`measured`).
struct Walked{
    std::optional<Pose> at;
    std::string error;
    std::string plan;
    bool measured = false;
    std::vector<std::string> golden = {};
};

//  The tiles a plan walks.
int tiles_walked(const std::vector<PlannedLeg>& plan){
    int tiles = 0;
    for (const PlannedLeg& leg : plan){
        tiles += leg.leg.tiles;
    }
    return tiles;
}

//  A route may walk a little further than the shortest way, for a wall at a
//  leg's end, but not round the town: r64 went 38 tiles to the School's door
//  for 12 straight (the "town" costs still feared stick-era open legs).
std::string detour_error(const StoryWorld& world, const std::vector<TilePos>& start,
    const std::function<bool(const TilePos&)>& at_goal, const std::vector<PlannedLeg>& plan
){
    const auto shortest = plan_robust_route(world, start, at_goal, waypoint_plan_costs());
    const int walked = tiles_walked(plan);
    const int least = shortest ? tiles_walked(*shortest) : walked;
    if (walked <= least + least / 2 + 4){
        return "";
    }
    return std::format("the plan walks {} tiles where {} would do.", walked, least);
}


//  The golden test's lines for one planned ROUTE: a header, then each leg
//  with the tiles it may end on.
void add_golden_lines(
    const StoryWorld& world, const Pose& from, const Pose& goal, const StoryStep& step,
    const std::vector<PlannedLeg>& plan, std::vector<std::string>& golden
){
    golden.push_back(std::format("{} costs={} from{} to{}", step.name, step.action.empty() ? "default" : step.action,
        describe(from.map, Pokemon::tiles_of(from)), describe(goal.map, Pokemon::tiles_of(goal))));
    for (const PlannedLeg& leg : plan){
        std::string line = std::format("  {} ->", to_string(leg.leg));
        for (const TilePos& pos : leg.after){
            line += std::format(" {}", describe(world, pos));
        }
        golden.push_back(line);
    }
}

//  A ROUTE step from `from`, planned as StoryExecutor::walk_to() walks it.
Walked plan_route(const Pose& from, const StoryStep& step){
    const GridPose& target = story_arg<GridPose>(step.args, "target");
    const Pose goal = Pokemon::pose_of(target);
    const StoryWorld* world = story_plan_world(from.map, goal.map);
    const std::optional<RobustPlanCosts> costs = story_route_costs(step.action);
    if (world == nullptr || !costs){
        return {std::nullopt, std::format("{}: no world joins {} and {}, or no costs \"{}\".", step.name, from.map, goal.map, step.action), ""};
    }
    std::vector<TilePos> start;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        start.push_back(story_to_world(*world, from.map, pos));
    }
    std::vector<TilePos> targets;
    for (const TilePos& pos : Pokemon::tiles_of(goal)){
        targets.push_back(story_to_world(*world, goal.map, pos));
    }
    const std::function<bool(const TilePos&)> at_goal = [&](const TilePos& pos){ return contains(targets, pos); };
    //  As walk_route() plans: from a wall when the start is one of a few
    //  tiles, then with the default costs if the step's find nothing. From
    //  01.03's two start tiles a search over both found no plan within its
    //  budget once routes kept off the hiker's zone.
    std::optional<std::vector<PlannedLeg>> plan = plan_recovered_route(*world, start, at_goal, *costs);
    if (!plan){
        plan = plan_recovered_route(*world, start, at_goal, RobustPlanCosts());
    }
    if (!plan){
        return {std::nullopt, step.name + ": no plan from" + describe(from.map, Pokemon::tiles_of(from)) + ".", ""};
    }
    Walked walked{std::nullopt, "", std::format("{}: {} legs", step.name, plan->size())};
    std::vector<TilePos> local;
    for (const PlannedLeg& leg : *plan){
        walked.plan += " " + to_string(leg.leg);
    }
    add_golden_lines(*world, from, goal, step, *plan, walked.golden);
    const std::string detour = detour_error(*world, start, at_goal, *plan);
    if (!detour.empty()){
        return {std::nullopt, step.name + ": " + detour, walked.plan};
    }
    for (const TilePos& pos : plan->empty() ? start : plan->back().after){
        const std::optional<TilePos> tile = story_to_local(*world, goal.map, pos);
        if (!tile || !contains(Pokemon::tiles_of(goal), *tile)){
            return {std::nullopt, step.name + ": the plan may end off the target, at " + describe(*world, pos) + ".", walked.plan};
        }
        local.push_back(*tile);
    }
    walked.at = Pokemon::tile_pose(goal.map, local, Pokemon::facing_of(goal));
    return walked;
}

//  A ROUTE step after a "measured" one: the program plans from the one
//  tile it measured, so each tile of `from` must plan on its own.
Walked plan_route_from_each(const Pose& from, const StoryStep& step){
    Walked walked;
    std::vector<TilePos> ends;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        const Walked one = plan_route(Pokemon::tile_pose(from.map, {pos}, Pokemon::facing_of(from)), step);
        if (!one.error.empty() || !one.at){
            return one;
        }
        walked.plan += (walked.plan.empty() ? "" : "; ") + one.plan;
        for (const TilePos& end : Pokemon::tiles_of(*one.at)){
            if (!contains(ends, end)){
                ends.push_back(end);
            }
        }
        walked.at = Pokemon::tile_pose(one.at->map, ends, Pokemon::facing_of(*one.at));
    }
    return walked;
}

//  `leg` of `step` from `pos`: adds where it ends to `next` (and a tile
//  short where the step allows it). Returns why it is blocked, or "".
std::string walk_leg_from(
    const StoryWorld& world, TilePos pos, const Pokemon::StoryLeg& leg, const StoryStep& step,
    std::vector<TilePos>& next
){
    for (int i = 0; i < leg.tiles; i++){
        const std::optional<TilePos> stepped = PokemonBDSP::step(world, pos, leg.direction);
        if (!stepped){
            return step.name + ": " + direction_name(leg.direction) + " is blocked at " + describe(world, pos) + ".";
        }
        if (step.action == "one_short_ok" && i + 1 == leg.tiles){
            next.push_back(pos);
        }
        pos = *stepped;
    }
    next.push_back(pos);
    return "";
}

//  A LEGS step from `from`, walked tile by tile on its map, a tile short too
//  where the step allows it. Walks to walls, and walks that end on another
//  map (into Sandgem until Dawn's scene starts), end where nothing is known.
Walked walk_legs(const Pose& from, const StoryStep& step){
    if (step.action == "to_wall"){
        return {std::nullopt, "", ""};
    }
    //  A run until a map's banner ends where the banner reads.
    if (step.action == "to_map"){
        return {step.pose_after ? std::optional<Pose>(Pokemon::pose_of(*step.pose_after)) : std::nullopt, "", ""};
    }
    if (step.action == "measured"){
        const std::optional<Pose> ends = measured_ends(from, step);
        return {ends, ends ? "" : step.name + ": no world holds " + from.map + ".", "", true};
    }
    const StoryWorld* world = story_plan_world(from.map, from.map);
    if (world == nullptr){
        return {std::nullopt, step.name + ": no world holds " + from.map + ".", ""};
    }
    const auto& legs = story_arg<std::vector<StoryLeg>>(step.args, "legs");
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        at.push_back(story_to_world(*world, from.map, pos));
    }
    for (const Pokemon::StoryLeg& leg : legs){
        std::vector<TilePos> next;
        for (const TilePos& pos : at){
            const std::string blocked = walk_leg_from(*world, pos, leg, step, next);
            if (!blocked.empty()){
                return {std::nullopt, blocked, ""};
            }
        }
        at = std::move(next);
    }
    //  Legs may cross onto a neighbouring map of the same world (Route 201's
    //  east end into Sandgem): the pose lands on the map the tiles are on.
    std::string map;
    std::vector<TilePos> local;
    for (const TilePos& pos : at){
        const std::optional<MapTile> tile = story_to_local(*world, pos);
        if (!tile){
            return {std::nullopt, step.name + ": the legs end off every map, at " + Pokemon::to_string(pos) + ".", ""};
        }
        if (!map.empty() && tile->map != map){
            return {std::nullopt, step.name + ": the legs end across " + map + " and " + tile->map + ".", ""};
        }
        map = tile->map;
        local.push_back(tile->pos);
    }
    return {Pokemon::tile_pose(map, local, legs.back().direction), "", ""};
}

//  A STEP onto the next tile of the same map.
Walked step_on(const Pose& from, const StoryStep& step){
    const Direction direction = story_arg<Direction>(step.args, "direction");
    std::vector<TilePos> at;
    for (const TilePos& pos : Pokemon::tiles_of(from)){
        at.push_back(pos.next(direction));
    }
    return {Pokemon::tile_pose(from.map, at, direction), "", ""};
}


//  The game: steps pass, routes, legs and steps move the pose on the maps,
//  and the steps in `failures` ("<row>/<step>") fail, so many times.
class FakeGame : public Pokemon::StoryStepExecutor{
public:
    struct Failure{
        std::string failure;
        size_t times = 0;
    };

    std::vector<std::string>& trace;
    std::map<std::string, Failure> failures;
    std::vector<std::string> plans;
    //  Every ROUTE's legs, one header line per route, for the golden test.
    std::vector<std::string> golden;

    explicit FakeGame(std::vector<std::string>& trace)
        : trace(trace)
    {}

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
        switch (step.kind){
        case StoryStepKind::ROUTE:
            return route(state, step);
        case StoryStepKind::LEGS:
            return legs(state, step);
        case StoryStepKind::STEP:
            return this->step(state, step);
        case StoryStepKind::SAVE:
            return save(state, step);
        default:
            return Pokemon::outcome_ok();
        }
    }
protected:
    Outcome route(RunState& state, const StoryStep& step){
        return moved(plan_route(state.pose, step), "no_plan");
    }
    Outcome legs(RunState& state, const StoryStep& step){
        Walked walked = walk_legs(state.pose, step);
        //  The fake player stands on the first tile a measured step may end on.
        if (walked.measured && walked.at && !Pokemon::tiles_of(*walked.at).empty()){
            walked.at = Pokemon::tile_pose(walked.at->map, {Pokemon::tiles_of(*walked.at).front()}, Pokemon::facing_of(*walked.at));
        }
        return moved(walked, "blocked");    }
    Outcome step(RunState& state, const StoryStep& step){
        return moved(step_on(state.pose, step), "blocked");
    }
    Outcome save(RunState&, const StoryStep& step){
        trace.push_back("save in " + m_row);
        if (!step.action.empty()){
            trace.push_back("card for " + m_row + ": " + step.action);
        }
        return Pokemon::outcome_ok();
    }
public:
    void set_language(PokemonAutomation::Language){}

private:
    Outcome moved(const Walked& walked, std::string_view failure){
        if (!walked.error.empty()){
            return Pokemon::outcome_failed(std::string(failure), walked.error);
        }
        if (!walked.plan.empty()){
            plans.push_back(walked.plan);
        }
        for (const std::string& line : walked.golden){
            golden.push_back(std::format("{} {}", m_row, line));
        }
        return walked.at ? Pokemon::outcome_ok(*walked.at) : Pokemon::outcome_ok(Pose{});
    }

    std::string m_row;
};


//  One simulated run, as the program runs it, in `language`. Japanese has
//  no text reader, so a step that needed text would show. (The console's
//  global language is left alone: tests run in parallel.)
struct Simulation{
    std::vector<std::string> trace;
    std::vector<std::string> lines;
    FakeGame game{trace};
    std::set<std::string> saved;
    Pokemon::SegmentPolicy policy;

    Pokemon::SegmentReport run(const Pokemon::SegmentRange& range, Language language = Language::Japanese){
        game.set_language(language);
        Pokemon::Segment segment = make_story_segment(story_checkpoints(), game, saved);
        Pokemon::SegmentHooks hooks;
        hooks.reload = [this](RunState&){
            trace.push_back("reload");
            return Pokemon::outcome_ok();
        };
        hooks.log = [this](const std::string& line){ lines.push_back(line); };
        RunState state;
        return Pokemon::run_segment(range, policy, segment, hooks, state);
    }
    std::vector<std::string> begun() const{
        std::vector<std::string> ids;
        for (const std::string& item : trace){
            if (item.starts_with("begin ")){
                ids.push_back(item.substr(6));
            }
        }
        return ids;
    }
    size_t count(const std::string& item) const{
        return (size_t)std::count(trace.begin(), trace.end(), item);
    }
    bool logged(const std::string& line) const{
        return std::find(lines.begin(), lines.end(), line) != lines.end();
    }
};

std::vector<std::string> all_ids(){
    std::vector<std::string> ids;
    for (const StoryCheckpoint& row : story_checkpoints()){
        ids.push_back(row.id);
    }
    return ids;
}


UnitTestResult test_whole_table(Logger& logger){
    for (Language language : {Language::English, Language::Japanese}){
        Simulation sim;
        const Pokemon::SegmentReport report = sim.run({}, language);
        for (const std::string& plan : sim.game.plans){
            logger.log(plan);
        }
        if (!report.completed){
            return std::string(language_data(language).name) + ": expected the whole story through:" + joined(sim.lines);
        }
        if (sim.begun() != all_ids()){
            return "Unexpected checkpoint order:" + joined(sim.begun());
        }
        //  Every row saves first but 00.00, which a failure ends; 01.10 and
        //  01.11 save again at their ends.
        for (const std::string& id : all_ids()){
            const size_t saves = sim.count("save in " + id);
            const size_t expected = id == "00.00" ? 0 : id == "01.10" || id == "01.11" ? 2 : 1;
            if (saves != expected){
                return std::format("{} saved {} times, expected {}:{}", id, saves, expected, joined(sim.trace));
            }
        }
        //  Each start save checks the card for its row's map.
        for (const StoryCheckpoint& row : story_checkpoints()){
            if (!row.saves_in.empty() && sim.count("card for " + row.id + ": " + row.saves_in) != 1){
                return std::format("{}'s start save did not check the card for {}:{}", row.id, row.saves_in, joined(sim.trace));
            }
        }
        if (report.one_shot_passes() != report.checkpoints_run){
            return "Expected every checkpoint one-shot:" + joined(sim.lines);
        }
    }
    return true;
}

//  The legs planned for every ROUTE of the whole story, against the stored
//  copy in PokemonBDSP_AutoStory_PlanGolden.h. A change that should not
//  alter any plan must leave this byte-identical. To regenerate after an
//  intended plan change: run this suite, and replace the lines of
//  BDSP_PLAN_GOLDEN with the lines the failure logs after "GOLDEN|".
UnitTestResult test_golden_plans(Logger& logger){
    Simulation sim;
    const Pokemon::SegmentReport report = sim.run({}, Language::English);
    if (!report.completed){
        return "expected the whole story through:" + joined(sim.lines);
    }
    const std::vector<std::string>& actual = sim.game.golden;
    const size_t stored = sizeof(BDSP_PLAN_GOLDEN) / sizeof(BDSP_PLAN_GOLDEN[0]);
    size_t first_diff = 0;
    while (first_diff < actual.size() && first_diff < stored && actual[first_diff] == BDSP_PLAN_GOLDEN[first_diff]){
        first_diff++;
    }
    if (first_diff == actual.size() && first_diff == stored){
        return true;
    }
    for (const std::string& line : actual){
        logger.log(std::format("GOLDEN|{}", line));
    }
    return std::format("plans differ from the golden at line {} ({} lines now, {} stored): now \"{}\", stored \"{}\".",
        first_diff + 1, actual.size(), stored,
        first_diff < actual.size() ? std::string_view(actual[first_diff]) : std::string_view(),
        first_diff < stored ? BDSP_PLAN_GOLDEN[first_diff] : std::string_view());
}

UnitTestResult test_start_and_end(Logger&){
    Simulation sim;
    Pokemon::SegmentReport report = sim.run({.from = "00.04b", .to = "00.04d"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"00.04b", "00.04c", "00.04d"}){
        return "Expected 00.04b to 00.04d only:" + joined(sim.begun());
    }
    Simulation tail;
    report = tail.run({.from = "01.10", .to = std::nullopt});
    if (!report.completed || tail.begun() != std::vector<std::string>{"01.10", "01.11"}){
        return "Expected 01.10 to the end:" + joined(tail.begun());
    }
    Simulation one;
    report = one.run({.from = "00.02", .to = "00.02"});
    if (!report.completed || one.begun() != std::vector<std::string>{"00.02"}){
        return "Expected 00.02 alone:" + joined(one.begun());
    }
    return true;
}

//  r72: a blackout in 00.04b's training. The checkpoint resets to the save
//  it made at its start, does not save again, and walks from that save's
//  pose to the grass again.
UnitTestResult test_training_blackout(Logger&){
    Simulation sim;
    sim.game.failures["00.04b/train"] = {.failure = "blackout", .times = 1};
    const Pokemon::SegmentReport report = sim.run({.from = "00.04b", .to = "00.04d"});
    const std::vector<std::string> expected{"00.04b", "00.04b", "00.04c", "00.04d"};
    if (!report.completed || report.reloads != 1 || sim.begun() != expected){
        return "Expected 00.04b reloaded once, then on:" + joined(sim.trace);
    }
    if (sim.count("save in 00.04b") != 1 || sim.count("00.04b/to_catch_grass") != 2){
        return "Expected the retry to start from 00.04b's one save and walk to the grass again:" + joined(sim.trace);
    }
    if (!sim.logged("[Checkpoint] 00.04b: passed on attempt 2 (1 reset)")){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

//  A step that fails every time: three attempts from the checkpoint's
//  save, then the segment stops on it and runs nothing after.
UnitTestResult test_gives_up_after_attempts(Logger&){
    const std::string step = "01.04/gym_until_roark_beaten";
    Simulation sim;
    sim.game.failures[step] = {.failure = "operation_failed", .times = 10};
    const Pokemon::SegmentReport report = sim.run({.from = "01.01", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "01.04" || report.failed_event != "gym_until_roark_beaten"
        || report.runs.empty() || report.runs.back().attempts != 3 || sim.count(step) != 3
        || sim.count("save in 01.04") != 1 || sim.count("begin 01.05") != 0
    ){
        return "Expected 01.04 tried 3 times from its save, then a stop:" + joined(sim.trace) + joined(sim.lines);
    }
    Simulation lab;
    lab.policy.attempts_per_checkpoint = 1;
    lab.game.failures[step] = {.failure = "operation_failed", .times = 10};
    const Pokemon::SegmentReport stopped = lab.run({.from = "01.01", .to = std::nullopt});
    if (stopped.completed || stopped.reloads != 0 || lab.count("reload") != 0){
        return "Expected a stop at the first failure without a reload:" + joined(lab.trace);
    }
    return true;
}

//  Each row that has a start pose starts where the row before it leaves
//  the player, by the same planning and walking the fake run does.
UnitTestResult test_poses_chain(Logger&){
    std::optional<Pose> at;
    bool measured = false;
    std::string last;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.start){
            const Pose start_pose = Pokemon::pose_of(*row.start);
            const std::vector<TilePos> start = Pokemon::tiles_of(start_pose);
            if (at && (at->map != Pokemon::grid_pose_of(*row.start).map ||
                !std::ranges::all_of(Pokemon::tiles_of(*at), [&](const TilePos& pos){ return contains(start, pos); }))
            ){
                return std::format("{} starts at{}, but {} may end at{}.", row.id, describe(Pokemon::grid_pose_of(*row.start).map, start),
                    last, describe(at->map, Pokemon::tiles_of(*at)));
            }
            at = start_pose;
        }else{
            at.reset();
        }
        for (const StoryStep& step : row.steps){
            if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS || step.kind == StoryStepKind::STEP){
                if (!at && step.action != "to_wall" && step.action != "jubilife_stairs"){
                    return row.id + " " + step.name + ": walks from an unknown pose.";
                }
                const bool known = std::exchange(measured, false);
                const Walked walked = !at ? Walked{} : step.kind == StoryStepKind::ROUTE
                    ? (known ? plan_route_from_each(*at, step) : plan_route(*at, step))
                    : step.kind == StoryStepKind::LEGS ? walk_legs(*at, step) : step_on(*at, step);
                if (!walked.error.empty()){
                    return row.id + " " + walked.error;
                }
                at = walked.at;
                measured = walked.measured;
            }else if (step.kind == StoryStepKind::GAME || step.kind == StoryStepKind::SHOP || step.kind == StoryStepKind::MASH){
                //  A scene or an event: where it ends is the table's to say.
                at.reset();
            }
            if (step.pose_after){
                at = Pokemon::pose_of(*step.pose_after);
            }
        }
        last = row.id;
    }
    return true;
}

//  Walks are planned on the maps: a LEGS step ("east 6, north 18") is
//  left only where no route can do its job, each with its reason.
UnitTestResult test_no_hand_written_legs(Logger&){
    const std::map<std::string, std::string> exceptions = {
        {"00.01/off_barrys_mat", "steps off a door's mat, where no route starts"},
        {"00.03/into_sandgem", "walks until Dawn's scene starts, wherever it starts"},
        {"00.04/to_route_201", "runs until Route 201's banner, wherever it shows"},
        {"00.04/to_moms_call", "walks until Mom calls, wherever she calls"},
        {"00.05/up_to_barry", "starts inside the School's door, from no known tile"},
        {"01.03/down_the_hall", "steps off the hall's warp tile, where no route starts"},
        {"01.03/down_to_the_boulder", "steps off the coal room's warp tile, where no route starts"},
        {"01.03/up_the_hall", "steps off the hall's warp tile, where no route starts"},
        {"01.03/up_to_the_mines_sign", "steps off the mine's exit, where no route starts"},
        {"01.11/up_to_the_old_woman", "walks until the old woman stops us"},
    };
    std::set<std::string> seen;
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const StoryStep& step : row.steps){
            if (step.kind != StoryStepKind::LEGS){
                continue;
            }
            const std::string key = std::format("{}/{}", row.id, step.name);
            if (!exceptions.contains(key)){
                return std::format("{}: hand-written legs; plan a route to where they end.", key);
            }
            seen.insert(key);
        }
    }
    for (const auto& item : exceptions){
        if (!seen.contains(item.first)){
            return std::format("{}: listed as an exception, but no such LEGS step.", item.first);
        }
    }
    return true;
}

//  Legs that run off the end of their map land on the next one of the same
//  world: east along Route 201's top row ends in Sandgem, not off the map.
UnitTestResult test_legs_cross_maps(Logger&){
    const Pose from = Pokemon::tile_pose("Route201", {{42, 5}}, Direction::EAST);
    StoryStep step{.kind = StoryStepKind::LEGS, .name = "east into Sandgem"};
    step.args["legs"] = std::vector<StoryLeg>{{Direction::EAST, 22}};
    const Walked walked = walk_legs(from, step);
    if (!walked.error.empty()){
        return walked.error;
    }
    if (!walked.at || walked.at->map != "SandgemTown"){
        return "The legs end on " + (walked.at ? walked.at->map : std::string("no pose")) + ", not SandgemTown.";
    }
    return true;
}

//  With no reader for the game's language, the reads that used text give
//  way: a banner or save card that showed matches any map.
UnitTestResult test_no_text_reader(Logger&){
    for (Language language : {Language::Japanese, Language::Korean, Language::French}){
        if (story_reads_text(language)){
            return std::string("Expected no text reader for ") + language_data(language).name + ".";
        }
    }
    if (!location_banner_matches(UNREAD_LOCATION, "Route 202") || location_banner_matches("", "Route 202")){
        return "Expected a shown, unread banner to match and no banner not to.";
    }
    if (!story_reads_text(Language::English) || location_banner_matches("sandgemtown", "Route 202")){
        return "Expected English banners read and compared.";
    }
    return true;
}


struct AutoStoryStorySimulationInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryStorySimulationExpectation{
};

struct AutoStoryStorySimulationCase{
    AutoStoryStorySimulationInput input;
    AutoStoryStorySimulationExpectation expectation;
};

class Test_StorySimulation : public UnitTest{
public:
    explicit Test_StorySimulation(AutoStoryStorySimulationCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Simulation: " + test_case.input.name)
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
    AutoStoryStorySimulationCase m_case;
};

} // namespace


//  Execute the authored mine resource through the actual table and shared engine.
UnitTestResult test_authored_mine(Logger&){
    Simulation sim;
    const auto report = sim.run({.from = "01.03", .to = "01.03"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"01.03"}
        || sim.count("save in 01.03") != 1 || sim.count("card for 01.03: Oreburgh City") != 1
        || sim.count("01.03/heal_oreburgh") != 1 || sim.count("01.03/roark") != 1){
        return "Expected the JSON-authored mine checkpoint to save once, talk to Roark, heal and finish:" + joined(sim.trace) + joined(sim.lines);
    }
    return true;
}

UnitTestResult test_authored_mine_stops(Logger&){
    Simulation sim;
    sim.game.failures["01.03/out_of_the_mine"] = {.failure = "unconfirmed_transition", .times = 3};
    const auto report = sim.run({.from = "01.03", .to = "01.03"});
    if (report.completed || report.reloads != 2 || report.failed_event != "out_of_the_mine"
        || sim.count("01.03/heal_oreburgh") != 0){
        return "Expected failed mine transition to exhaust retries before healing:" + joined(sim.trace) + joined(sim.lines);
    }
    return true;
}

const AutoStoryStorySimulationCase AUTO_STORY_STORY_SIMULATION_CASES[] = {
    {.input = {.name = "01.03 authored mine executes through the real table", .body = test_authored_mine}, .expectation = {}},
    {.input = {.name = "01.03 unconfirmed mine exit stops before healing", .body = test_authored_mine_stops}, .expectation = {}},
    {
        .input = {"00.00 to Floaroma, in order, each row saved first, English and Japanese", test_whole_table},
        .expectation = {},
    },
    {
        .input = {"start and end checkpoints", test_start_and_end},
        .expectation = {},
    },
    {
        .input = {"a blackout in 00.04b's training resets to its save", test_training_blackout},
        .expectation = {},
    },
    {
        .input = {"a step that keeps failing stops after 3 attempts", test_gives_up_after_attempts},
        .expectation = {},
    },
    {
        .input = {"each start pose is where the row before ends; routes plan on the maps", test_poses_chain},
        .expectation = {},
    },
    {
        .input = {"legs that run off their map land on the next", test_legs_cross_maps},
        .expectation = {},
    },
    {
        .input = {"no text reader for the game's language", test_no_text_reader},
        .expectation = {},
    },
    {
        .input = {"no hand-written legs but the listed ones", test_no_hand_written_legs},
        .expectation = {},
    },
    {
        .input = {"plans match the golden", test_golden_plans},
        .expectation = {},
    },
};

void add_tests_AutoStory_StorySimulation(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_SIMULATION_CASES){
        database.add<Test_StorySimulation>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
