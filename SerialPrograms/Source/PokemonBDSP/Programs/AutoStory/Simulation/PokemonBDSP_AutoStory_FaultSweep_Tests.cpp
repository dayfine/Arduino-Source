/*  BDSP AutoStory - Fault sweeps on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The shared fault sweep (Pokemon_StoryFaultSweep.h) over BDSP's rows:
 *  each class of failure console B showed, injected at every step of every
 *  row where the game can cause it, instead of only where it was seen. A
 *  test is named by the shared class's id ("00.04, wild_battles"). BDSP's
 *  sim injects:
 *
 *  - SCENE_END_POSE: a step whose end pose the game sets (a talk, a scene,
 *    a battle, an event) leaves the player on another walkable tile within
 *    one of the table's (00.03, lab-B 2026-11-09: Rowan's scene ended a row
 *    low). SimWorldKnobs::lands.
 *  - BATTLE_OPENING: a battle's opening is still playing when a step starts
 *    (00.02's Starly, lab-B 2026-11-09), at each battle step and at each
 *    step after a walk, which can end in a trainer's sight or in grass.
 *    SimWorldKnobs::battle_opening_before.
 *  - WILD_BATTLES: a wild battle on every pass through tall grass, one
 *    each 8 grass tiles (00.04, lab-B 2026-11-09 16:14: four in a row on
 *    Route 201, each replan back through the grass, "too many plans").
 *    SimWorldKnobs::grass. One variant a row: the whole row so.
 *  - SCENE_EARLY: a scene starts early, on the last tiles of the route walk
 *    before its step, or before the plain step onto its trigger: the
 *    trigger lies on the way, not where the decomp has it (00.04's Mom's call on
 *    (6, 9), lab-B 2026-11-09 17:29: the walk took it for a trainer and
 *    pressed through it). A variant also fails when the walk fights it.
 *    SimWorldKnobs::scene_at_walk_end.
 *  - SLOW_FADE_IN: every warp's fade-in runs long, so a move after a fade
 *    with no settle() between goes nowhere (lab-B 2026-11-10, 00.04b
 *    heal_sandgem: a fixed 1500 ms wait, then the walk to the nurse
 *    stopped on "transition"). SimWorldKnobs::slow_fade_in.
 *
 *  Missed and short fades are swept by SimFailureCases_Tests (every single
 *  missed fade, banner and fade hold). Not injected yet, so not swept:
 *  lost_tap and unseen_step (the measured movement table, FRLG #293),
 *  mount_lost (the sim has no Bicycle), late_fade, warp_no_op and
 *  dark_map.
 *
 *  Each row is run clean first; a row that fails clean is not swept. A
 *  variant fails when the row then fails. A sweep the program fails today
 *  is on KNOWN_FAILING with its likely fix: its test passes while it still
 *  fails, and fails once it passes, to be taken off the list.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryFaultSweep.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimFaults.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_WalkRules.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryPlaces.h"
#include "PokemonBDSP_AutoStory_StorySim.h"
#include "PokemonBDSP_AutoStory_FaultSweep_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

namespace{

using Pokemon::Direction;
using Pokemon::Pose;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;
using Pokemon::TilePos;

using Pokemon::FaultClass;
using Pokemon::FaultSweepResult;

//  The start of every test's name.
constexpr std::string_view TEST_PREFIX = "PokemonBDSP::AutoStory";
//  The rows swept: those the clean simulation runs (SimFailureCases_Tests).
constexpr std::string_view FIRST_ROW = "00.00";
constexpr std::string_view LAST_ROW = "01.11";

//  Sweeps the program fails today, by test name, with the likely fix.
const std::map<std::string, std::string> KNOWN_FAILING{
};

//  Scenes whose end the console has proven, by step name: the sweep leaves
//  them out, as the tiles next to it are not where the game leaves us.
const std::map<std::string, std::string> PROVEN_SCENE_ENDS{
    //  Mom's Running Shoes scene: (6, 8) facing east in 7 of 7 runs stopped
    //  right after it (lab-B 2026-11-09). The rows 7, 9 and 10 seen before
    //  were our input landing in its closing fade, now waited out.
    {"rowan_and_dawn", "lab-B 2026-11-09, 7 of 7 runs on (6, 8)"},
};

//  Runs row `row` with `knobs`: "" if it passes, else where and why it stopped.
std::string run_row(Logger& logger, const std::string& row, SimWorldKnobs knobs){
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row, .to = row});
    return report.completed ? "" : std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
}

//  Whether the game, not our walking, sets where `kind` ends.
bool game_sets_end(StoryStepKind kind){
    switch (kind){
    case StoryStepKind::NEW_GAME:
    case StoryStepKind::TALK:
    case StoryStepKind::ANSWER:
    case StoryStepKind::MASH:
    case StoryStepKind::BATTLE:
    case StoryStepKind::GAME:
        return true;
    default:
        return false;
    }
}
bool walks(StoryStepKind kind){
    return kind == StoryStepKind::ROUTE || kind == StoryStepKind::LEGS || kind == StoryStepKind::STEP;
}
//  Whether row `row` run clean up to its step `c` leaves the player on one
//  of `pose`'s tiles: a talk, a trainer's battle or a scene that only turns
//  the player ends where our walk did, so its end is not the game's.
bool already_there(Logger& logger, const StoryCheckpoint& row, size_t c, const Pose& pose){
    StoryCheckpoint head = row;
    head.steps.resize(c);
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    if (!run_rows_on(executor, logger, {.from = row.id, .to = row.id}, {head}).completed || executor.result().world == nullptr){
        return false;
    }
    const SimRun& run = executor.result();
    for (const TilePos& tile : Pokemon::tiles_of(pose)){
        try{
            if (story_to_world(*run.world, pose.map, tile) == run.end){
                return true;
            }
        }catch (const InternalProgramError&){}
    }
    return false;
}
//  Whether walk `step` ends where a battle can start: on a route (grass, a
//  trainer's sight) or in a cave. Towns and rooms have neither.
bool wild_or_trainers(const StoryStep& step){
    std::string map;
    if (step.pose_after){
        map = Pokemon::grid_pose_of(*step.pose_after).map;
    }else if (step.args.contains("target")){
        map = Pokemon::story_arg<Pokemon::GridPose>(step.args, "target").map;
    }else{
        return true;
    }
    for (std::string_view place : {"Route", "Gate", "Mine", "RavagedPath"}){
        if (map.find(place) != std::string::npos){
            return true;
        }
    }
    return false;
}

//  The tiles within one of `center`, diagonals too, that the player can
//  reach from it in one or two steps, and not in `seen`: where a scene can
//  leave them instead. A tile behind a wall is not one, nor a door's: a
//  step onto it goes through (00.04's moms_hat by the house's door).
void walkable_neighbors(const StoryWorld* world, const Pose& pose, const TilePos& center, std::set<std::pair<int, int>>& seen, std::vector<TilePos>& out){
    const auto step = [](const TilePos& tile, Direction direction){
        switch (direction){
        case Direction::NORTH:
            return TilePos{tile.x, tile.y - 1};
        case Direction::SOUTH:
            return TilePos{tile.x, tile.y + 1};
        case Direction::WEST:
            return TilePos{tile.x - 1, tile.y};
        default:
            return TilePos{tile.x + 1, tile.y};
        }
    };
    const auto can_step = [&](const TilePos& from, Direction direction){
        GridSim sim(SimWorld{.world = world}, story_to_world(*world, pose.map, from), direction, false);
        return detail::sim_can_step(sim, direction);
    };
    const Pokemon::MapData& map = story_map(pose.map);
    const auto add = [&](const TilePos& tile){
        if (map.tiles.at(tile) == Pokemon::TileKind::DOOR){
            return;
        }
        if (seen.insert({tile.x, tile.y}).second){
            out.push_back(tile);
        }
    };
    const Direction all[] = {Direction::NORTH, Direction::SOUTH, Direction::WEST, Direction::EAST};
    for (Direction first : all){
        if (!can_step(center, first)){
            continue;
        }
        const TilePos one = step(center, first);
        add(one);
        for (Direction second : all){
            const TilePos two = step(one, second);
            const bool diagonal = two.x != center.x && two.y != center.y;
            if (diagonal && can_step(one, second)){
                add(two);
            }
        }
    }
}
//  The walkable tiles next to any of `pose`'s tiles, not its own.
std::vector<TilePos> walkable_neighbors(const Pose& pose){
    const std::vector<TilePos> tiles = Pokemon::tiles_of(pose);
    const StoryWorld* world = pose.map.empty() ? nullptr : story_plan_world(pose.map, pose.map);
    std::vector<TilePos> out;
    if (world == nullptr){
        return out;
    }
    std::set<std::pair<int, int>> seen;
    for (const TilePos& tile : tiles){
        seen.insert({tile.x, tile.y});
    }
    for (const TilePos& tile : tiles){
        walkable_neighbors(world, pose, tile, seen, out);
    }
    return out;
}

//  Row `row`'s steps whose end the game sets, each ending on every walkable
//  tile next to the table's pose in turn.
FaultSweepResult sweep_scene_end_pose(Logger& logger, const StoryCheckpoint& row){
    FaultSweepResult result;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        if (!game_sets_end(step.kind) || !step.pose_after || PROVEN_SCENE_ENDS.contains(step.name) ||
            already_there(logger, row, c, Pokemon::pose_of(*step.pose_after))){
            continue;
        }
        for (const TilePos& tile : walkable_neighbors(Pokemon::pose_of(*step.pose_after))){
            SimWorldKnobs knobs;
            knobs.lands[step.name] = Pokemon::tile_pose(Pokemon::grid_pose_of(*step.pose_after).map, {tile}, Pokemon::facing_of(Pokemon::pose_of(*step.pose_after)));
            result.variants++;
            const std::string stopped = run_row(logger, row.id, std::move(knobs));
            if (!stopped.empty()){
                result.failures.push_back({.step = step.name, .variant = std::format("ends on ({}, {})", tile.x, tile.y), .stopped = stopped});
            }
        }
    }
    return result;
}

//  Row `row` with a battle's opening still playing at the start of each
//  battle step and of each step after a walk, one at a time.
FaultSweepResult sweep_battle_opening(Logger& logger, const StoryCheckpoint& row){
    FaultSweepResult result;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        const bool after_walk = c > 0 && walks(row.steps[c - 1].kind) && wild_or_trainers(row.steps[c - 1]);
        if (step.kind != StoryStepKind::BATTLE && !after_walk){
            continue;
        }
        SimWorldKnobs knobs;
        knobs.battle_opening_before = [id = row.id, name = step.name](const std::string& r, const std::string& s){
            return r == id && s == name;
        };
        result.variants++;
        const std::string stopped = run_row(logger, row.id, std::move(knobs));
        if (!stopped.empty()){
            result.failures.push_back({.step = step.name, .variant = "opening at its start", .stopped = stopped});
        }
    }
    return result;
}

//  Row `row` with a wild battle each 8 tall-grass tiles stepped onto, all
//  through the row.
FaultSweepResult sweep_grass_battles(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.grass.every = 8;
    const std::string stopped = run_row(logger, row.id, std::move(knobs));
    FaultSweepResult result;
    result.variants = 1;
    if (!stopped.empty()){
        result.failures.push_back({.step = "", .variant = "a battle each 8 grass tiles", .stopped = stopped});
    }
    return result;
}

//  Row `row` with every warp's fade-in slow, all through the row.
FaultSweepResult sweep_slow_fade_in(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.slow_fade_in = true;
    const std::string stopped = run_row(logger, row.id, std::move(knobs));
    FaultSweepResult result;
    result.variants = 1;
    if (!stopped.empty()){
        result.failures.push_back({.step = "", .variant = "a slow fade-in after every warp", .stopped = stopped});
    }
    return result;
}

//  The route before scene step `c` of `steps`, right before it or before
//  the plain step onto its trigger; none if there is none.
std::optional<size_t> route_before_scene(const std::vector<StoryStep>& steps, size_t c){
    if (steps[c].kind != StoryStepKind::MASH || c == 0){
        return std::nullopt;
    }
    if (steps[c - 1].kind == StoryStepKind::ROUTE){
        return c - 1;
    }
    const StoryStep& step = steps[c - 1];
    const bool onto_trigger = step.kind == StoryStepKind::STEP && step.action.empty() && !step.pose_after;
    if (onto_trigger && c >= 2 && steps[c - 2].kind == StoryStepKind::ROUTE){
        return c - 2;
    }
    return std::nullopt;
}

//  Row `row` with the scene of each scene step after a route starting on
//  that route's target, one at a time.
FaultSweepResult sweep_scene_early(Logger& logger, const StoryCheckpoint& row){
    FaultSweepResult result;
    for (size_t c = 1; c < row.steps.size(); c++){
        const std::optional<size_t> w = route_before_scene(row.steps, c);
        if (!w){
            continue;
        }
        const StoryStep& walk = row.steps[*w];
        SimWorldKnobs knobs;
        knobs.scene_at_walk_end = [id = row.id, name = walk.name](const std::string& r, const std::string& s){
            return r == id && s == name;
        };
        result.variants++;
        SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
        const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row.id, .to = row.id});
        const SimRun& run = executor.result();
        const std::string early = "early scene after " + walk.name;
        const bool fought = std::any_of(run.scenes_fought.begin(), run.scenes_fought.end(), [&](const std::string& scene){
            return scene.ends_with(early);
        });
        const std::string variant = "starts on " + walk.name + "'s end";
        if (!report.completed){
            result.failures.push_back({.step = row.steps[c].name, .variant = variant, .stopped = std::format("stopped in {}: {}", run.failed_step, run.error)});
        }else if (fought){
            result.failures.push_back({.step = row.steps[c].name, .variant = variant, .stopped = "the walk fought it as a trainer's"});
        }
    }
    return result;
}

//  The table's row `id`.
const StoryCheckpoint& table_row(const std::string& id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No row " + id);
}

//  The injector of one sim fault: `sweep` over the table's row, unless the
//  row fails clean.
Pokemon::StoryFaultSweep::Injector injector(FaultSweepResult (*sweep)(Logger&, const StoryCheckpoint&)){
    return [sweep](Logger& logger, const std::string& id){
        const StoryCheckpoint& row = table_row(id);
        const std::string clean = run_row(logger, row.id, {});
        if (!clean.empty()){
            FaultSweepResult result;
            result.not_swept = "the row fails clean: " + clean;
            return result;
        }
        return sweep(logger, row);
    };
}

//  The table's rows from FIRST_ROW to LAST_ROW, in story order.
std::vector<std::string> swept_rows(){
    std::vector<std::string> rows;
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id >= FIRST_ROW && row.id <= LAST_ROW){
            rows.push_back(row.id);
        }
    }
    return rows;
}

std::shared_ptr<const Pokemon::StoryFaultSweep> bdsp_fault_sweep(){
    auto sweep = std::make_shared<Pokemon::StoryFaultSweep>();
    sweep->test_prefix = std::string(TEST_PREFIX);
    sweep->rows = swept_rows();
    sweep->injectors = {
        {FaultClass::SCENE_END_POSE, injector(sweep_scene_end_pose)},
        {FaultClass::BATTLE_OPENING, injector(sweep_battle_opening)},
        {FaultClass::WILD_BATTLES, injector(sweep_grass_battles)},
        {FaultClass::SCENE_EARLY, injector(sweep_scene_early)},
        {FaultClass::SLOW_FADE_IN, injector(sweep_slow_fade_in)},
    };
    sweep->known_failing = KNOWN_FAILING;
    return sweep;
}

} // namespace


void add_tests_AutoStory_FaultSweep(UnitTestDatabase& database){
    Pokemon::add_fault_sweep_tests(database, bdsp_fault_sweep());
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
