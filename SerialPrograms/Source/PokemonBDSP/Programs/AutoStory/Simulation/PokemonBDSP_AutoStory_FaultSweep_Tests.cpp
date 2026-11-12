/*  BDSP AutoStory - Fault sweeps on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Each class of failure console B showed, injected at every step of every
 *  row where the game can cause it, instead of only where it was seen:
 *
 *  - SCENE_END_POSE: a step whose end pose the game sets (a talk, a scene,
 *    a battle, an event) leaves the player on another walkable tile within
 *    one of the table's (00.03, lab-B 2026-11-09: Rowan's scene ended a row
 *    low). SimWorldKnobs::lands.
 *  - BATTLE_OPENING: a battle's opening is still playing when a step starts
 *    (00.02's Starly, lab-B 2026-11-09), at each battle step and at each
 *    step after a walk, which can end in a trainer's sight or in grass.
 *    SimWorldKnobs::battle_opening_before.
 *  - GRASS_BATTLES: a wild battle on every pass through tall grass, one
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
 *  - START_OFFSET: the row's save leaves the player 1 to 3 tiles off its
 *    declared start, each way the player can walk there (00.05, lab-B
 *    2026-11-11 15:22: the route planned open legs from Jubilife's declared
 *    (48, 62), and its third leg walked into a building).
 *    SimWorldKnobs::start_at.
 *
 *  Missed and short fades are swept by SimFailureCases_Tests (every single
 *  missed fade, banner and fade hold).
 *  A lost tap from the input lockout waits for the measured movement
 *  table (FRLG #293); FaultClass::LOST_TAP is its slot.
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
#include <set>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryCertainty.h"
#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStory_StoryData.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
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

enum class FaultClass{
    SCENE_END_POSE,
    BATTLE_OPENING,
    GRASS_BATTLES,
    SCENE_EARLY,
    SLOW_FADE_IN,
    START_OFFSET,
    //  Not swept yet: the measured movement table's lost taps (#293).
    LOST_TAP,
};
std::string_view fault_class_name(FaultClass fault){
    switch (fault){
    case FaultClass::SCENE_END_POSE:
        return "scene end pose";
    case FaultClass::BATTLE_OPENING:
        return "battle opening at step start";
    case FaultClass::GRASS_BATTLES:
        return "wild battles on every grass pass";
    case FaultClass::SCENE_EARLY:
        return "scene starts during the walk before it";
    case FaultClass::SLOW_FADE_IN:
        return "slow fade-in after every warp";
    case FaultClass::START_OFFSET:
        return "start off its declared tile";
    case FaultClass::LOST_TAP:
        return "lost tap";
    }
    return "?";
}

//  The rows swept: those the clean simulation runs (SimFailureCases_Tests).
constexpr std::string_view FIRST_ROW = "00.00";
constexpr std::string_view LAST_ROW = "01.11";

//  Sweeps the program fails today, by test name, with the likely fix.
const std::map<std::string, std::string> KNOWN_FAILING{
    {"PokemonBDSP::AutoStory - Fault sweep: 00.01, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.02, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.03, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.04, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.04b, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.05, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.05b, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 00.05c, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.01, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.02, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.02b, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.03, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.05, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.06, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.07, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.08, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.09, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.10, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
    {"PokemonBDSP::AutoStory - Fault sweep: 01.11, start off its declared tile", "a first leg to a wall or funnel that pins the start (as 00.05's walk to the Center)"},
};

//  Steps that need a surer pose than the steps before them leave, from
//  the table before certainty_gaps(): each is to start on a wall leg or
//  a landmark, as 00.05's walk to the Center does.
const std::set<std::string> KNOWN_CERTAINTY_GAPS{
    "00.01 step 1 below_the_flute: needs tile, has set",
    "00.01 step 12 into_barrys_house: needs tile, has set",
    "00.01 step 19 down_barrys_stairs: needs tile, has set",
    "00.01 step 23 barrys_warning: needs tile, has set",
    "00.01 step 26 below_the_lake_entrance: needs tile, has set",
    "00.01 step 4 down_the_stairs: needs tile, has set",
    "00.01 step 6 above_the_door: needs tile, has set",
    "00.02 step 1 to_the_lake_entrance: needs tile, has set",
    "00.03 step 1 into_rowans_scene: needs tile, has set",
    "00.03 step 3 out_of_the_house_again: needs tile, has set",
    "00.04 step 1 out_of_the_lab: needs tile, has set",
    "00.04 step 18 short_of_the_catch_tutorial: needs tile, has set",
    "00.04 step 2 onto_the_path: needs tile, has set",
    "00.04 step 5 to_ledge_column: needs tile, has set",
    "00.04 step 7 along_route_201_road: needs tile, has set",
    "00.04b step 1 to_catch_grass: needs tile, has set",
    "00.04b step 3 into_sandgem: needs tile, has set",
    "00.04d step 1 north_to_jubilife: needs tile, has set",
    "00.05 step 10 barry: needs tile, has unknown",
    "00.05 step 12 out_of_the_school: needs tile, has set",
    "00.05b step 1 below_the_presidents_trigger: needs tile, has set",
    "00.05b step 10 below_the_company_clown: needs tile, has set",
    "00.05b step 4 past_the_president: needs tile, has set",
    "00.05c step 6 to_the_route_203_street: needs tile, has set",
    "01.01 step 2 west_of_barrys_trigger: needs tile, has set",
    "01.02 step 1 south_to_the_cliff: needs tile, has set",
    "01.02 step 10 west_of_the_welcome: needs tile, has set",
    "01.02 step 4 west_of_the_hikers_trigger: needs tile, has set",
    "01.02 step 7 past_curtis: needs tile, has set",
    "01.02b step 2 barry: needs tile, has set",
    "01.02b step 4 below_the_centers_door: needs tile, has set",
    "01.03 step 1 above_the_mine: needs tile, has set",
    "01.03 step 11 up_to_the_hall: needs tile, has set",
    "01.03 step 15 heal_oreburgh: needs tile, has set",
    "01.03 step 8 roark: needs tile, has set",
    "01.05 step 2 east_of_barrys_trigger: needs tile, has set",
    "01.05 step 8 through_the_gate: needs tile, has set",
    "01.06 step 1 up_jubilifes_road: needs tile, has set",
    "01.07 step 1 rowans_trigger: needs tile, has set",
    "01.08 step 1 west_to_the_wall: needs tile, has set",
    "01.09 step 1 west_of_the_rock: needs tile, has set",
    "01.09 step 3 above_the_way_out: needs tile, has set",
    "01.10 step 1 line_1: needs tile, has set",
    "01.11 step 7 old_man: needs tile, has set",
};

//  Scenes whose end the console has proven, by step name: the sweep leaves
//  them out, as the tiles next to it are not where the game leaves us.
const std::map<std::string, std::string> PROVEN_SCENE_ENDS{
    //  Mom's Running Shoes scene: (6, 8) facing east in 7 of 7 runs stopped
    //  right after it (lab-B 2026-11-09). The rows 7, 9 and 10 seen before
    //  were our input landing in its closing fade, now waited out.
    {"rowan_and_dawn", "lab-B 2026-11-09, 7 of 7 runs on (6, 8)"},
};

//  A talk that walks to its end itself: wait_for_wanderer counts its taps
//  along one column and ends with taps into two walls that must not move,
//  so a tile beside its end fails the step on the console.
constexpr std::string_view WALKS_TO_ITS_END = "wait_for_wanderer";

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
//  The tile next to `tile` toward `direction`.
TilePos step_from(const TilePos& tile, Direction direction){
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
}

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

//  One injected variant that failed its row.
struct SweepFailure{
    std::string step;
    std::string variant;
    std::string stopped;
};

UnitTestResult sweep_result(Logger& logger, FaultClass fault, const std::string& row, size_t variants, const std::vector<SweepFailure>& failures){
    logger.log(std::format("Fault sweep {} {}: {} variants, {} fail.", row, fault_class_name(fault), variants, failures.size()));
    if (failures.empty()){
        return true;
    }
    std::string out = std::format("{} of {} variants fail {}:", failures.size(), variants, row);
    for (const SweepFailure& failure : failures){
        logger.log(std::format("| {} | {} | {} | {} | {} |", row, failure.step, fault_class_name(fault), failure.variant, failure.stopped));
        out += std::format("\n  {} {}: {}", failure.step, failure.variant, failure.stopped);
    }
    return out;
}

//  Row `row`'s steps whose end the game sets, each ending on every walkable
//  tile next to the table's pose in turn.
UnitTestResult sweep_scene_end_pose(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepFailure> failures;
    size_t variants = 0;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        if (!game_sets_end(step.kind) || step.action == WALKS_TO_ITS_END || !step.pose_after || PROVEN_SCENE_ENDS.contains(step.name) ||
            already_there(logger, row, c, Pokemon::pose_of(*step.pose_after))){
            continue;
        }
        for (const TilePos& tile : walkable_neighbors(Pokemon::pose_of(*step.pose_after))){
            SimWorldKnobs knobs;
            knobs.lands[step.name] = Pokemon::tile_pose(Pokemon::grid_pose_of(*step.pose_after).map, {tile}, Pokemon::facing_of(Pokemon::pose_of(*step.pose_after)));
            variants++;
            const std::string stopped = run_row(logger, row.id, std::move(knobs));
            if (!stopped.empty()){
                failures.push_back({step.name, std::format("ends on ({}, {})", tile.x, tile.y), stopped});
            }
        }
    }
    return sweep_result(logger, FaultClass::SCENE_END_POSE, row.id, variants, failures);
}

//  Row `row` with a battle's opening still playing at the start of each
//  battle step and of each step after a walk, one at a time.
UnitTestResult sweep_battle_opening(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepFailure> failures;
    size_t variants = 0;
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
        variants++;
        const std::string stopped = run_row(logger, row.id, std::move(knobs));
        if (!stopped.empty()){
            failures.push_back({step.name, "opening at its start", stopped});
        }
    }
    return sweep_result(logger, FaultClass::BATTLE_OPENING, row.id, variants, failures);
}

//  Row `row` with a wild battle each 8 tall-grass tiles stepped onto, all
//  through the row.
UnitTestResult sweep_grass_battles(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.grass.every = 8;
    const std::string stopped = run_row(logger, row.id, std::move(knobs));
    std::vector<SweepFailure> failures;
    if (!stopped.empty()){
        failures.push_back({"", "a battle each 8 grass tiles", stopped});
    }
    return sweep_result(logger, FaultClass::GRASS_BATTLES, row.id, 1, failures);
}

//  Row `row` with every warp's fade-in slow, all through the row.
UnitTestResult sweep_slow_fade_in(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.slow_fade_in = true;
    const std::string stopped = run_row(logger, row.id, std::move(knobs));
    std::vector<SweepFailure> failures;
    if (!stopped.empty()){
        failures.push_back({"", "a slow fade-in after every warp", stopped});
    }
    return sweep_result(logger, FaultClass::SLOW_FADE_IN, row.id, 1, failures);
}

//  Row `row` from 1 to 3 tiles off its declared start, each way the
//  player can walk from it, one at a time.
UnitTestResult sweep_start_offset(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepFailure> failures;
    size_t variants = 0;
    if (!row.start){
        return sweep_result(logger, FaultClass::START_OFFSET, row.id, variants, failures);
    }
    const Pose start = Pokemon::pose_of(*row.start);
    const std::vector<TilePos> tiles = Pokemon::tiles_of(start);
    const StoryWorld* world = start.map.empty() ? nullptr : story_plan_world(start.map, start.map);
    if (world == nullptr || tiles.empty()){
        return sweep_result(logger, FaultClass::START_OFFSET, row.id, variants, failures);
    }
    for (Direction direction : {Direction::NORTH, Direction::SOUTH, Direction::WEST, Direction::EAST}){
        TilePos at = tiles.front();
        for (int off = 1; off <= 3; off++){
            GridSim sim(SimWorld{.world = world}, story_to_world(*world, start.map, at), direction, false);
            if (!detail::sim_can_step(sim, direction)){
                break;
            }
            at = step_from(at, direction);
            SimWorldKnobs knobs;
            knobs.start_at = Pokemon::tile_pose(start.map, {at}, Pokemon::facing_of(start));
            variants++;
            const std::string stopped = run_row(logger, row.id, std::move(knobs));
            if (!stopped.empty()){
                failures.push_back({"", std::format("starts on ({}, {})", at.x, at.y), stopped});
            }
        }
    }
    return sweep_result(logger, FaultClass::START_OFFSET, row.id, variants, failures);
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
UnitTestResult sweep_scene_early(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepFailure> failures;
    size_t variants = 0;
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
        variants++;
        SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
        const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row.id, .to = row.id});
        const SimRun& run = executor.result();
        const std::string early = "early scene after " + walk.name;
        const bool fought = std::any_of(run.scenes_fought.begin(), run.scenes_fought.end(), [&](const std::string& scene){
            return scene.ends_with(early);
        });
        if (!report.completed){
            failures.push_back({row.steps[c].name, "starts on " + walk.name + "'s end", std::format("stopped in {}: {}", run.failed_step, run.error)});
        }else if (fought){
            failures.push_back({row.steps[c].name, "starts on " + walk.name + "'s end", "the walk fought it as a trainer's"});
        }
    }
    return sweep_result(logger, FaultClass::SCENE_EARLY, row.id, variants, failures);
}

//  A fault class swept over row `row` (empty in FAULT_SWEEP_CASES: each
//  row from FIRST_ROW to LAST_ROW).
struct FaultSweepInput{
    FaultClass fault = FaultClass::SCENE_END_POSE;
    std::string row = {};
};
//  Whether a sweep is expected to fail is KNOWN_FAILING's, by name.
struct FaultSweepExpectation{
};
struct FaultSweepCase{
    FaultSweepInput input;
    FaultSweepExpectation expectation;
};

//  The table's row `id`.
const StoryCheckpoint& table_row(const std::string& id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No row " + id);
}

class Test_AutoStoryFaultSweep : public UnitTest{
public:
    explicit Test_AutoStoryFaultSweep(FaultSweepCase test_case)
        : UnitTest(std::format("PokemonBDSP::AutoStory - Fault sweep: {}, {}", test_case.input.row, fault_class_name(test_case.input.fault)))
        , m_fault(test_case.input.fault)
        , m_row(table_row(test_case.input.row))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        UnitTestResult result;
        try{
            result = sweep(logger);
        }catch (Exception& e){
            result = std::string(e.name()) + ": " + e.message();
        }
        const auto known = KNOWN_FAILING.find(name());
        if (known == KNOWN_FAILING.end()){
            return result;
        }
        if (result.result == UnitTestResult::PASSED){
            return "on KNOWN_FAILING (" + known->second + ") but passes now: take it off the list.";
        }
        logger.log("Known failing until " + known->second + ": " + result.message);
        return true;
    }

private:
    UnitTestResult sweep(Logger& logger) const{
        const std::string clean = run_row(logger, m_row.id, {});
        if (!clean.empty()){
            logger.log(std::format("Fault sweep {}: not swept, the row fails clean: {}", m_row.id, clean));
            return true;
        }
        switch (m_fault){
        case FaultClass::SCENE_END_POSE:
            return sweep_scene_end_pose(logger, m_row);
        case FaultClass::BATTLE_OPENING:
            return sweep_battle_opening(logger, m_row);
        case FaultClass::GRASS_BATTLES:
            return sweep_grass_battles(logger, m_row);
        case FaultClass::SCENE_EARLY:
            return sweep_scene_early(logger, m_row);
        case FaultClass::SLOW_FADE_IN:
            return sweep_slow_fade_in(logger, m_row);
        case FaultClass::START_OFFSET:
            return sweep_start_offset(logger, m_row);
        case FaultClass::LOST_TAP:
            break;
        }
        return true;
    }

    FaultClass m_fault;
    const StoryCheckpoint& m_row;
};

//  No step of the table needs a surer pose than the steps before it leave
//  (Pokemon_StoryCertainty.h), but those on KNOWN_CERTAINTY_GAPS.
class Test_AutoStoryCertainty : public UnitTest{
public:
    Test_AutoStoryCertainty()
        : UnitTest("PokemonBDSP::AutoStory - Certainty: no step walks from a pose surer than the steps before leave")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        std::string out;
        size_t known = 0;
        for (const StoryCheckpoint& row : story_checkpoints()){
            for (const std::string& gap : Pokemon::certainty_gaps(row, bdsp_story_actions())){
                logger.log("Certainty gap: " + gap);
                if (KNOWN_CERTAINTY_GAPS.contains(gap)){
                    known++;
                    continue;
                }
                out += "\n  " + gap;
            }
        }
        if (known != KNOWN_CERTAINTY_GAPS.size()){
            out += std::format("\n  {} of the {} known gaps are gone: take them off KNOWN_CERTAINTY_GAPS.", KNOWN_CERTAINTY_GAPS.size() - known, KNOWN_CERTAINTY_GAPS.size());
        }
        return out.empty() ? UnitTestResult(true) : UnitTestResult("Certainty gaps:" + out);
    }
};

} // namespace

const FaultSweepCase FAULT_SWEEP_CASES[] = {
    {.input = {.fault = FaultClass::SCENE_END_POSE}, .expectation = {}},
    {.input = {.fault = FaultClass::BATTLE_OPENING}, .expectation = {}},
    {.input = {.fault = FaultClass::GRASS_BATTLES}, .expectation = {}},
    {.input = {.fault = FaultClass::SCENE_EARLY}, .expectation = {}},
    {.input = {.fault = FaultClass::SLOW_FADE_IN}, .expectation = {}},
    {.input = {.fault = FaultClass::START_OFFSET}, .expectation = {}},
};

void add_tests_AutoStory_FaultSweep(UnitTestDatabase& database){
    database.add<Test_AutoStoryCertainty>();
    for (const FaultSweepCase& item : FAULT_SWEEP_CASES){
        for (const StoryCheckpoint& row : story_checkpoints()){
            if (row.id < FIRST_ROW || row.id > LAST_ROW){
                continue;
            }
            database.add<Test_AutoStoryFaultSweep>(FaultSweepCase{.input = {.fault = item.input.fault, .row = row.id}, .expectation = item.expectation});
        }
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
