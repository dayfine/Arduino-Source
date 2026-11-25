/*  BDSP AutoStory Story Simulation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of the whole story table, new game to Floaroma, without the
 *  console: story_checkpoints() run through #648's shared story engine
 *  (Pokemon::run_checkpoints()) with BDSP's E9 save-at-end policy and retry
 *  policy, against a fake game.
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
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Engine/Pokemon_StoryEngine.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryPlanCosts.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryRobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryWalkRules.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStoryWallRecovery.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlanGolden.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryActions.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryPlaces.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTable.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryWalkSteps.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryLocation.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStoryLanguage.h"
#include "PokemonBDSP_AutoStorySim.h"
#include "PokemonBDSP_AutoStorySimulation_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::GridPose;
using Pokemon::StoryFailure;
using Pokemon::StoryLeg;
using Pokemon::TilePos;

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

struct Walked{
    std::optional<GridPose> at;
    std::string error;
    std::string plan;
    bool measured = false;
    std::vector<std::string> golden = {};
};

size_t tiles_walked(const std::vector<PlannedLeg>& plan){
    size_t tiles = 0;
    for (const PlannedLeg& leg : plan){
        tiles += leg.leg.tiles;
    }
    return tiles;
}

std::string detour_error(const StoryWorld& world, const std::vector<TilePos>& start,
    const std::function<bool(const TilePos&)>& at_goal, const std::vector<PlannedLeg>& plan
){
    const auto shortest = plan_robust_route(world, start, at_goal, waypoint_plan_costs());
    const size_t walked = tiles_walked(plan);
    const size_t least = shortest ? tiles_walked(*shortest) : walked;
    if (walked <= least + least / 2 + 4){
        return "";
    }
    return std::format("the plan walks {} tiles where {} would do.", walked, least);
}

void add_golden_lines(
    const StoryWorld& world, const GridPose& from, const GridPose& goal, const BdspStep& step,
    const std::string& variant, const std::vector<PlannedLeg>& plan, std::vector<std::string>& golden
){
    golden.push_back(std::format("{} costs={} from{} to{}", step.name, variant.empty() ? "default" : variant,
        describe(from.map, from.tiles), describe(goal.map, goal.tiles)));
    for (const PlannedLeg& leg : plan){
        std::string line = std::format("  {} ->", to_string(leg.leg));
        for (const TilePos& pos : leg.after){
            line += std::format(" {}", describe(world, pos));
        }
        golden.push_back(line);
    }
}

Walked plan_route(const GridPose& from, const BdspStep& step){
    const RouteStepArgs& args = std::get<RouteStepArgs>(step.args);
    const GridPose& goal = args.target;
    const StoryWorld* world = story_plan_world(from.map, goal.map);
    const std::optional<RobustPlanCosts> costs = story_route_costs(args.variant);
    if (world == nullptr || !costs){
        return {std::nullopt, std::format("{}: no world joins {} and {}, or no costs \"{}\".", step.name, from.map, goal.map, args.variant), ""};
    }
    std::vector<TilePos> start;
    for (const TilePos& pos : from.tiles){
        start.push_back(story_to_world(*world, from.map, pos));
    }
    std::vector<TilePos> targets;
    for (const TilePos& pos : goal.tiles){
        targets.push_back(story_to_world(*world, goal.map, pos));
    }
    const std::function<bool(const TilePos&)> at_goal = [&](const TilePos& pos){ return contains(targets, pos); };
    std::optional<std::vector<PlannedLeg>> plan = plan_recovered_route(*world, start, at_goal, *costs);
    if (!plan){
        plan = plan_recovered_route(*world, start, at_goal, RobustPlanCosts());
    }
    if (!plan){
        return {std::nullopt, step.name + ": no plan from" + describe(from.map, from.tiles) + ".", ""};
    }
    Walked walked{std::nullopt, "", std::format("{}: {} legs", step.name, plan->size())};
    std::vector<TilePos> local;
    for (const PlannedLeg& leg : *plan){
        walked.plan += " " + to_string(leg.leg);
    }
    add_golden_lines(*world, from, goal, step, args.variant, *plan, walked.golden);
    const std::string detour = detour_error(*world, start, at_goal, *plan);
    if (!detour.empty()){
        return {std::nullopt, step.name + ": " + detour, walked.plan};
    }
    for (const TilePos& pos : plan->empty() ? start : plan->back().after){
        const std::optional<TilePos> tile = story_to_local(*world, goal.map, pos);
        if (!tile || !contains(goal.tiles, *tile)){
            return {std::nullopt, step.name + ": the plan may end off the target, at " + describe(*world, pos) + ".", walked.plan};
        }
        local.push_back(*tile);
    }
    walked.at = GridPose{.map = goal.map, .tiles = std::move(local), .facing = goal.facing};
    return walked;
}

Walked plan_route_from_each(const GridPose& from, const BdspStep& step){
    Walked walked;
    std::vector<TilePos> ends;
    for (const TilePos& pos : from.tiles){
        const Walked one = plan_route(GridPose{.map = from.map, .tiles = {pos}, .facing = from.facing}, step);
        if (!one.error.empty() || !one.at){
            return one;
        }
        walked.plan += (walked.plan.empty() ? "" : "; ") + one.plan;
        for (const TilePos& end : one.at->tiles){
            if (!contains(ends, end)){
                ends.push_back(end);
            }
        }
        walked.at = GridPose{.map = one.at->map, .tiles = ends, .facing = one.at->facing};
    }
    return walked;
}

std::string walk_leg_from(
    const StoryWorld& world, TilePos pos, const StoryLeg& leg, const std::string& step_name,
    const std::string& variant, std::vector<TilePos>& next
){
    for (int i = 0; i < leg.tiles; i++){
        const std::optional<TilePos> stepped = PokemonBDSP::step(world, pos, leg.direction);
        if (!stepped){
            return step_name + ": " + direction_name(leg.direction) + " is blocked at " + describe(world, pos) + ".";
        }
        if (variant == "one_short_ok" && i + 1 == leg.tiles){
            next.push_back(pos);
        }
        pos = *stepped;
    }
    next.push_back(pos);
    return "";
}

Walked walk_legs(const GridPose& from, const BdspStep& step){
    const LegsStepArgs& args = std::get<LegsStepArgs>(step.args);
    if (args.variant == "to_wall"){
        return {std::nullopt, "", ""};
    }
    if (args.variant == "to_map"){
        return {step.pose_after, "", ""};
    }
    if (args.variant == "measured"){
        const std::optional<GridPose> ends = measured_ends(from, step);
        return {ends, ends ? "" : step.name + ": no world holds " + from.map + ".", "", true};
    }
    const StoryWorld* world = story_plan_world(from.map, from.map);
    if (world == nullptr){
        return {std::nullopt, step.name + ": no world holds " + from.map + ".", ""};
    }
    std::vector<TilePos> at;
    for (const TilePos& pos : from.tiles){
        at.push_back(story_to_world(*world, from.map, pos));
    }
    for (const StoryLeg& leg : args.legs){
        std::vector<TilePos> next;
        for (const TilePos& pos : at){
            const std::string blocked = walk_leg_from(*world, pos, leg, step.name, args.variant, next);
            if (!blocked.empty()){
                return {std::nullopt, blocked, ""};
            }
        }
        at = std::move(next);
    }
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
    return {GridPose{.map = map, .tiles = std::move(local), .facing = args.legs.back().direction}, "", ""};
}

Walked step_on(const GridPose& from, const BdspStep& step){
    const Direction direction = std::get<StepMoveArgs>(step.args).direction;
    std::vector<TilePos> at;
    for (const TilePos& pos : from.tiles){
        at.push_back(pos.next(direction));
    }
    return {GridPose{.map = from.map, .tiles = std::move(at), .facing = direction}, "", ""};
}

//  Fake game running BDSP checkpoints directly through #648 Pokemon::run_checkpoints()
//  with BDSP's E9 save-at-end policy.
struct FakeSimReport{
    bool completed = false;
    size_t reloads = 0;
    std::string failed_checkpoint = {};
    std::string failed_event = {};
    BdspRunState final_state = {};
};

struct Simulation{
    struct Failure{
        std::string failure;
        size_t times = 0;
    };

    std::vector<std::string> trace;
    std::vector<std::string> lines;
    std::map<std::string, Failure> failures;
    std::vector<std::string> plans;
    std::vector<std::string> golden;
    size_t attempts_per_checkpoint = 3;

    enum class StartMode{ SAVED, NEW_GAME };

private:
    std::optional<StoryFailure> run_fake_step(
        const StoryCheckpoint& row, size_t index, const BdspStep& step, BdspRunState& state
    ){
        if (index == 0){
            state.pose = row.start.value_or(GridPose{});
            trace.push_back("begin " + row.id);
        }
        const std::string key = row.id + "/" + step.name;
        trace.push_back(key);
        auto fail_it = failures.find(key);
        if (fail_it != failures.end() && fail_it->second.times > 0){
            fail_it->second.times--;
            return StoryFailure{
                .code = fail_it->second.failure,
                .detail = key,
                .repeats = row.on_failure == StoryFailurePolicy::STOP,
            };
        }
        std::optional<StoryFailure> err;
        if (step.action == "route"){
            err = apply_walk(row.id, plan_route(state.pose, step), "no_plan", state);
        }else if (step.action == "legs"){
            Walked walked = walk_legs(state.pose, step);
            if (walked.measured && walked.at && !walked.at->tiles.empty()){
                walked.at = GridPose{.map = walked.at->map, .tiles = {walked.at->tiles.front()}, .facing = walked.at->facing};
            }
            err = apply_walk(row.id, walked, "blocked", state);
        }else if (step.action == "step"){
            err = apply_walk(row.id, step_on(state.pose, step), "blocked", state);
        }else if (step.action == "save"){
            trace.push_back("step save in " + row.id);
        }
        if (!err && step.pose_after){
            state.pose = *step.pose_after;
        }
        return err;
    }

    std::vector<Pokemon::StoryCheckpoint> build_fake_checkpoints(
        const StoryRunPlan& plan, BdspRunState& state
    ){
        std::vector<Pokemon::StoryCheckpoint> checkpoints;
        checkpoints.reserve(plan.rows.size());
        for (const StoryCheckpoint& src : plan.rows){
            Pokemon::StoryCheckpoint cp;
            cp.id = src.id;
            cp.title = src.title;
            for (size_t i = 0; i < src.steps.size(); i++){
                const BdspStep step = src.steps[i];
                Pokemon::StoryStep core_step;
                core_step.name = step.name;
                core_step.why = step.why;
                core_step.action = step.action;
                core_step.run = [this, row = src, i, step, &state](Pokemon::StoryBackend&, Pokemon::RunState&){
                    return run_fake_step(row, i, step, state);
                };
                cp.steps.push_back(std::move(core_step));
            }
            checkpoints.push_back(std::move(cp));
        }
        return checkpoints;
    }

    Pokemon::StoryGame make_fake_game(
        const StoryRunPlan& plan, BdspRunState& state,
        std::optional<std::string>& saved_start, size_t& reloads
    ){
        Pokemon::StoryGame game;
        game.attempts_per_checkpoint = attempts_per_checkpoint;
        game.save = [&, this](Pokemon::StoryBackend&, const Pokemon::StoryCheckpoint& passed) -> std::optional<StoryFailure>{
            const auto it = std::find_if(plan.rows.begin(), plan.rows.end(), [&](const StoryCheckpoint& row){
                return row.id == passed.id;
            });
            if (it != plan.rows.end() && it + 1 != plan.rows.end()){
                const StoryCheckpoint& next = *(it + 1);
                if (next.on_failure == StoryFailurePolicy::RELOAD_AND_RETRY){
                    trace.push_back("end save for " + next.id);
                    trace.push_back("card for " + next.id + ": " + next.saves_in);
                    saved_start = next.id;
                }
                return std::nullopt;
            }
            if (plan.stop == StoryHandoff::STARTER_RNG && passed.id == STARTER_RNG_HANDOFF_ROW && !plan.handoff_saves_in.empty()){
                trace.push_back("handoff save after " + passed.id);
                trace.push_back("card for handoff: " + plan.handoff_saves_in);
            }
            return std::nullopt;
        };
        game.reload = [&, this](Pokemon::StoryBackend&, const Pokemon::StoryCheckpoint&) -> std::optional<std::string>{
            if (!saved_start){
                return std::nullopt;
            }
            reloads++;
            trace.push_back("reload");
            state = BdspRunState{.pose = {}, .save_loaded = true};
            return saved_start;
        };
        return game;
    }

public:
    FakeSimReport run_plan(const StoryRunPlan& plan, StartMode start_mode = StartMode::SAVED){
        const bool new_game = start_mode == StartMode::NEW_GAME;
        CancellableHolder<CancellableScope> scope;
        BdspLogBackend backend([this](std::string_view line){ lines.emplace_back(line); }, scope);
        BdspRunState state{.pose = {}, .save_loaded = !new_game};
        std::optional<std::string> saved_start = new_game ? std::nullopt : std::optional<std::string>(plan.rows.front().id);
        size_t reloads = 0;

        std::vector<Pokemon::StoryCheckpoint> checkpoints = build_fake_checkpoints(plan, state);
        Pokemon::StoryGame game = make_fake_game(plan, state, saved_start, reloads);
        Pokemon::RunState engine_state;
        const std::optional<Pokemon::CheckpointFailure> failure = Pokemon::run_checkpoints(
            backend, checkpoints, game, engine_state
        );
        if (!failure){
            return FakeSimReport{.completed = true, .reloads = reloads, .final_state = state};
        }
        std::string failed_event;
        const auto row_it = std::find_if(plan.rows.begin(), plan.rows.end(), [&](const StoryCheckpoint& row){
            return row.id == failure->checkpoint_id;
        });
        if (failure->event_index && row_it != plan.rows.end() && *failure->event_index < row_it->steps.size()){
            failed_event = row_it->steps[*failure->event_index].name;
        }
        return FakeSimReport{
            .completed = false,
            .reloads = reloads,
            .failed_checkpoint = failure->checkpoint_id,
            .failed_event = std::move(failed_event),
            .final_state = state,
        };
    }

    FakeSimReport run(const SimRunRange& range, Language = Language::Japanese){
        const std::vector<StoryCheckpoint>& all = story_checkpoints();
        size_t first = 0;
        size_t last = all.size() - 1;
        if (range.from){
            for (size_t i = 0; i < all.size(); i++){
                if (all[i].id == *range.from){
                    first = i;
                    break;
                }
            }
        }
        if (range.to){
            for (size_t i = first; i < all.size(); i++){
                if (all[i].id == *range.to){
                    last = i;
                    break;
                }
            }
        }
        return run_plan(plan_story_run(all, first, last), StartMode::SAVED);
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

private:
    std::optional<StoryFailure> apply_walk(const std::string& row_id, const Walked& walked, std::string_view code, BdspRunState& state){
        if (!walked.error.empty()){
            return StoryFailure{.code = std::string(code), .detail = walked.error};
        }
        if (!walked.plan.empty()){
            plans.push_back(walked.plan);
        }
        for (const std::string& line : walked.golden){
            golden.push_back(std::format("{} {}", row_id, line));
        }
        state.pose = walked.at.value_or(GridPose{});
        return std::nullopt;
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
        const FakeSimReport report = sim.run({}, language);
        for (const std::string& plan : sim.plans){
            logger.log(plan);
        }
        if (!report.completed){
            return std::string(language_data(language).name) + ": expected the whole story through:" + joined(sim.lines);
        }
        if (sim.begun() != all_ids()){
            return "Unexpected checkpoint order:" + joined(sim.begun());
        }
        //  Under E9, checkpoint N saves at its end for checkpoint N+1
        //  (every row except 00.00 receives an end-save from its predecessor).
        for (const StoryCheckpoint& row : story_checkpoints()){
            const size_t expected_end_saves = row.id == "00.00" ? 0 : 1;
            if (sim.count("end save for " + row.id) != expected_end_saves){
                return std::format("{} received {} end-saves, expected {}:{}", row.id, sim.count("end save for " + row.id), expected_end_saves, joined(sim.trace));
            }
            if (!row.saves_in.empty() && sim.count("card for " + row.id + ": " + row.saves_in) != 1){
                return std::format("{}'s E9 save did not check the card for {}:{}", row.id, row.saves_in, joined(sim.trace));
            }
        }
        if (report.reloads != 0){
            return "Expected every checkpoint one-shot:" + joined(sim.lines);
        }
    }
    return true;
}

UnitTestResult test_golden_plans(Logger& logger){
    Simulation sim;
    const FakeSimReport report = sim.run({}, Language::English);
    if (!report.completed){
        return "expected the whole story through:" + joined(sim.lines);
    }
    const std::vector<std::string>& actual = sim.golden;
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
    FakeSimReport report = sim.run({.from = "00.04b", .to = "00.04d"});
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
    //  With Stop for Starter RNG on, a 00.00..01.11 run stops after 00.01's
    //  E9 handoff save at VerityLakefront (17, 14).
    Simulation handoff_sim;
    const StoryRunPlan plan = plan_story_run(
        story_checkpoints(), 0, story_checkpoints().size() - 1, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::STOP}
    );
    const FakeSimReport handoff_report = handoff_sim.run_plan(plan, Simulation::StartMode::NEW_GAME);
    if (!handoff_report.completed || plan.stop != StoryHandoff::STARTER_RNG ||
        handoff_sim.begun() != std::vector<std::string>{"00.00", "00.01"} ||
        handoff_sim.count("handoff save after 00.01") != 1 ||
        handoff_sim.count("card for handoff: Verity Lakefront") != 1 ||
        handoff_report.final_state.pose.map != "VerityLakefront" ||
        handoff_report.final_state.pose.tiles != std::vector<TilePos>{{17, 14}}
    ){
        return std::format("Expected Starter RNG handoff to stop after 00.01 saved at VerityLakefront (17, 14):{}", joined(handoff_sim.trace));
    }
    return true;
}

UnitTestResult test_run_starter_rng_in_story(Logger&){
    Simulation in_program_sim;
    const StoryRunPlan in_program_plan = plan_story_run(
        story_checkpoints(), 0, 3, StoryHandoffs{.starter_rng = Pokemon::RngHandoffMode::RUN}
    );
    const FakeSimReport in_program_report = in_program_sim.run_plan(in_program_plan, Simulation::StartMode::NEW_GAME);
    if (!in_program_report.completed ||
        in_program_sim.begun() != std::vector<std::string>{"00.00", "00.01", "00.02", "00.03"} ||
        in_program_sim.count("end save for 00.02") != 1 ||
        in_program_sim.count("card for 00.02: Verity Lakefront") != 1 ||
        in_program_sim.count("00.02/starter_rng") != 1 ||
        in_program_sim.count("00.02/choose_starter") != 0 ||
        in_program_sim.count("end save for 00.03") != 1 ||
        in_program_sim.count("card for 00.03: Verity Lakefront") != 1
    ){
        return std::format(
            "Expected 00.02 with Run Starter RNG to run 00.02/starter_rng and E9-save into 00.03:{}",
            joined(in_program_sim.trace)
        );
    }
    return true;
}

UnitTestResult test_training_blackout(Logger&){
    Simulation sim;
    sim.failures["00.04b/train"] = {.failure = "blackout", .times = 1};
    const FakeSimReport report = sim.run({.from = "00.04b", .to = "00.04d"});
    const std::vector<std::string> expected{"00.04b", "00.04b", "00.04c", "00.04d"};
    if (!report.completed || report.reloads != 1 || sim.begun() != expected){
        return "Expected 00.04b reloaded once, then on:" + joined(sim.trace);
    }
    if (sim.count("00.04b/to_catch_grass") != 2){
        return "Expected the retry to walk to the grass again:" + joined(sim.trace);
    }
    if (!sim.logged("[Checkpoint] 00.04b: passed on attempt 2 (1 reset)")){
        return "Expected the retried pass logged:" + joined(sim.lines);
    }
    return true;
}

UnitTestResult test_gives_up_after_attempts(Logger&){
    const std::string step = "01.04/gym_until_roark_beaten";
    Simulation sim;
    sim.failures[step] = {.failure = "operation_failed", .times = 10};
    const FakeSimReport report = sim.run({.from = "01.01", .to = std::nullopt});
    if (report.completed || report.reloads != 2 || report.failed_checkpoint != "01.04" || report.failed_event != "gym_until_roark_beaten"
        || sim.count(step) != 3 || sim.count("end save for 01.04") != 1 || sim.count("begin 01.05") != 0
    ){
        return "Expected 01.04 tried 3 times from its save, then a stop:" + joined(sim.trace) + joined(sim.lines);
    }
    Simulation lab;
    lab.attempts_per_checkpoint = 1;
    lab.failures[step] = {.failure = "operation_failed", .times = 10};
    const FakeSimReport stopped = lab.run({.from = "01.01", .to = std::nullopt});
    if (stopped.completed || stopped.reloads != 0 || lab.count("reload") != 0){
        return "Expected a stop at the first failure without a reload:" + joined(lab.trace);
    }
    return true;
}

UnitTestResult test_poses_chain(Logger&){
    std::optional<GridPose> at;
    bool measured = false;
    std::string last;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.start){
            const GridPose& start_pose = *row.start;
            if (at && (at->map != start_pose.map ||
                !std::ranges::all_of(at->tiles, [&](const TilePos& pos){ return contains(start_pose.tiles, pos); }))
            ){
                return std::format("{} starts at{}, but {} may end at{}.", row.id, describe(start_pose.map, start_pose.tiles),
                    last, describe(at->map, at->tiles));
            }
            at = start_pose;
        }else{
            at.reset();
        }
        for (const BdspStep& step : row.steps){
            if (step.action == "route" || step.action == "legs" || step.action == "step"){
                const bool is_to_wall = step.action == "legs" && std::get<LegsStepArgs>(step.args).variant == "to_wall";
                if (!at && !is_to_wall){
                    return row.id + " " + step.name + ": walks from an unknown pose.";
                }
                const bool known = std::exchange(measured, false);
                const Walked walked = !at ? Walked{} : step.action == "route"
                    ? (known ? plan_route_from_each(*at, step) : plan_route(*at, step))
                    : step.action == "legs" ? walk_legs(*at, step) : step_on(*at, step);
                if (!walked.error.empty()){
                    return row.id + " " + walked.error;
                }
                at = walked.at;
                measured = walked.measured;
            }else if (step.action == "scene" || step.action == "sandgem_potions" || step.action == "jubilife_stairs" ||
                step.action == "train_fire_starter_in_mine" || step.action == "gym_until_roark_beaten" ||
                step.action == "smash_the_rock" ||
                step.action == "azure_flute_if_there" || step.action == "starter_briefcase" ||
                step.action == "choose_starter" || step.action == "catch_tutorial" ||
                step.action == "catch_pokemon" || step.action == "talk_to_wanderer"
            ){
                at.reset();
            }
            if (step.pose_after){
                at = *step.pose_after;
            }
        }
        last = row.id;
    }
    return true;
}

UnitTestResult test_no_hand_written_legs(Logger&){
    for (const StoryCheckpoint& row : story_checkpoints()){
        for (const BdspStep& step : row.steps){
            if (step.action == "legs" && step.why.empty()){
                return std::format("{}/{}: hand-written legs with no why; plan a route to where they end, or say why none can.",
                    row.id, step.name);
            }
        }
    }
    return true;
}

UnitTestResult test_legs_cross_maps(Logger&){
    const GridPose from{.map = "Route201", .tiles = {{42, 5}}, .facing = Direction::EAST};
    BdspStep step{
        .name = "east into Sandgem",
        .action = "legs",
        .args = LegsStepArgs{.legs = {{Direction::EAST, 22}}},
    };
    const Walked walked = walk_legs(from, step);
    if (!walked.error.empty()){
        return walked.error;
    }
    if (!walked.at || walked.at->map != "SandgemTown"){
        return "The legs end on " + (walked.at ? walked.at->map : std::string("no pose")) + ", not SandgemTown.";
    }
    return true;
}

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


struct AutoStoryStorySimulationCase{
    struct Input{
        std::function<UnitTestResult(Logger&)> body;
    };
    struct Expectation{
    };
    std::string name;
    Input input;
    Expectation expectation;
};

class Test_StorySimulation : public UnitTest{
public:
    explicit Test_StorySimulation(AutoStoryStorySimulationCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - Simulation: " + test_case.name)
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

UnitTestResult test_authored_mine(Logger&){
    Simulation sim;
    const FakeSimReport report = sim.run({.from = "01.03", .to = "01.03"});
    if (!report.completed || sim.begun() != std::vector<std::string>{"01.03"}
        || sim.count("01.03/heal_oreburgh") != 1 || sim.count("01.03/roark") != 1){
        return "Expected the JSON-authored mine checkpoint to talk to Roark, heal and finish:" + joined(sim.trace) + joined(sim.lines);
    }
    return true;
}

UnitTestResult test_authored_mine_stops(Logger&){
    Simulation sim;
    sim.failures["01.03/out_of_the_mine"] = {.failure = "unconfirmed_transition", .times = 3};
    const FakeSimReport report = sim.run({.from = "01.03", .to = "01.03"});
    if (report.completed || report.reloads != 2 || report.failed_event != "out_of_the_mine"
        || sim.count("01.03/heal_oreburgh") != 0){
        return "Expected failed mine transition to exhaust retries before healing:" + joined(sim.trace) + joined(sim.lines);
    }
    return true;
}

} // namespace


const AutoStoryStorySimulationCase AUTO_STORY_STORY_SIMULATION_CASES[] = {
    {.name = "01.03 authored mine executes through the real table", .input = {.body = test_authored_mine}, .expectation = {}},
    {.name = "01.03 unconfirmed mine exit stops before healing", .input = {.body = test_authored_mine_stops}, .expectation = {}},
    {.name = "00.00 to Floaroma, in order, each row E9-saved at end, English and Japanese", .input = {test_whole_table}, .expectation = {}},
    {.name = "start and end checkpoints", .input = {test_start_and_end}, .expectation = {}},
    {.name = "Run Starter RNG in AutoStory chains 00.02 into 00.03", .input = {test_run_starter_rng_in_story}, .expectation = {}},
    {.name = "a blackout in 00.04b's training resets to its save", .input = {test_training_blackout}, .expectation = {}},
    {.name = "a step that keeps failing stops after 3 attempts", .input = {test_gives_up_after_attempts}, .expectation = {}},
    {.name = "each start pose is where the row before ends; routes plan on the maps", .input = {test_poses_chain}, .expectation = {}},
    {.name = "legs that run off their map land on the next", .input = {test_legs_cross_maps}, .expectation = {}},
    {.name = "no text reader for the game's language", .input = {test_no_text_reader}, .expectation = {}},
    {.name = "no hand-written legs without a why", .input = {test_no_hand_written_legs}, .expectation = {}},
    {.name = "plans match the golden", .input = {test_golden_plans}, .expectation = {}},
};

void add_tests_AutoStory_StorySimulation(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_STORY_SIMULATION_CASES){
        database.add<Test_StorySimulation>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
