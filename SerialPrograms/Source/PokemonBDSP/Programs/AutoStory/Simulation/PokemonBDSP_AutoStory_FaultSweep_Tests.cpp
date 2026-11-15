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
 *
 *  Missed and short fades are swept by SimFailureCases_Tests (every single
 *  missed fade, banner and fade hold).
 *  A lost tap from the input lockout waits for the measured movement
 *  table (FRLG #293); FaultClass::LOST_TAP is its slot.
 *
 *  Each row is run clean first; a row that fails clean is not swept. A
 *  variant fails when the row then fails. A sweep the program fails today
 *  is on known_failing() with its likely fix: its test passes while it
 *  still fails, and fails once it passes, to be taken off the list.
 *
 *  The runner (variants, clean first, the known-failing list) is the
 *  shared one (Pokemon_SimSweep.h, Pokemon_KnownFailing.h); this file
 *  keeps BDSP's rows and how each class runs on its sim.
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
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "Pokemon/Navigation/Simulation/Pokemon_KnownFailing.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimSweep.h"
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
using Pokemon::SweepVariant;
using Pokemon::TilePos;

enum class FaultClass{
    SCENE_END_POSE,
    BATTLE_OPENING,
    GRASS_BATTLES,
    SCENE_EARLY,
    SLOW_FADE_IN,
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
    case FaultClass::LOST_TAP:
        return "lost tap";
    }
    return "?";
}

//  The rows swept: those the clean simulation runs (SimFailureCases_Tests).
constexpr std::string_view FIRST_ROW = "00.00";
constexpr std::string_view LAST_ROW = "01.11";

//  Sweeps the program fails today, by test name, with the likely fix.
const Pokemon::KnownFailing& known_failing(){
    static const Pokemon::KnownFailing list({});
    return list;
}

//  Scenes whose end the console has proven, by step name: the sweep leaves
//  them out, as the tiles next to it are not where the game leaves us.
const std::map<std::string, std::string> PROVEN_SCENE_ENDS{
    //  Mom's Running Shoes scene: (6, 8) facing east in 7 of 7 runs stopped
    //  right after it (lab-B 2026-11-09). The rows 7, 9 and 10 seen before
    //  were our input landing in its closing fade, now waited out.
    {"rowan_and_dawn", "lab-B 2026-11-09, 7 of 7 runs on (6, 8)"},
};

//  Runs row `row` with `knobs`: where and why it stopped, nothing if it
//  passes.
std::optional<std::string> run_row(Logger& logger, const std::string& row, SimWorldKnobs knobs){
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row, .to = row});
    if (report.completed){
        return std::nullopt;
    }
    return std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
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

//  The sweep of `fault` over row `row` with `variants`.
UnitTestResult sweep_variants(Logger& logger, const StoryCheckpoint& row, FaultClass fault, const std::vector<SweepVariant>& variants){
    return Pokemon::run_sweep(logger, row.id, fault_class_name(fault), variants);
}

//  A variant: row `row` run with `knobs`.
SweepVariant row_variant(Logger& logger, std::string step, std::string label, const StoryCheckpoint& row, SimWorldKnobs knobs){
    return {std::move(step), std::move(label), [&logger, &row, knobs = std::move(knobs)]{
        return run_row(logger, row.id, knobs);
    }};
}

//  Row `row`'s step `c` ending on `tile` next to the table's pose.
SweepVariant end_pose_variant(Logger& logger, const StoryCheckpoint& row, size_t c, const TilePos& tile){
    const StoryStep& step = row.steps[c];
    SimWorldKnobs knobs;
    knobs.lands[step.name] = Pokemon::tile_pose(Pokemon::grid_pose_of(*step.pose_after).map, {tile}, Pokemon::facing_of(Pokemon::pose_of(*step.pose_after)));
    return row_variant(logger, step.name, std::format("ends on ({}, {})", tile.x, tile.y), row, std::move(knobs));
}

//  Row `row`'s steps whose end the game sets, each ending on every walkable
//  tile next to the table's pose in turn.
UnitTestResult sweep_scene_end_pose(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
    for (size_t c = 0; c < row.steps.size(); c++){
        const StoryStep& step = row.steps[c];
        if (!game_sets_end(step.kind) || !step.pose_after || PROVEN_SCENE_ENDS.contains(step.name) ||
            already_there(logger, row, c, Pokemon::pose_of(*step.pose_after))){
            continue;
        }
        for (const TilePos& tile : walkable_neighbors(Pokemon::pose_of(*step.pose_after))){
            variants.push_back(end_pose_variant(logger, row, c, tile));
        }
    }
    return sweep_variants(logger, row, FaultClass::SCENE_END_POSE, variants);
}

//  Row `row` with a battle's opening still playing at the start of each
//  battle step and of each step after a walk, one at a time.
UnitTestResult sweep_battle_opening(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
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
        variants.push_back(row_variant(logger, step.name, "opening at its start", row, std::move(knobs)));
    }
    return sweep_variants(logger, row, FaultClass::BATTLE_OPENING, variants);
}

//  Row `row` with a wild battle each 8 tall-grass tiles stepped onto, all
//  through the row.
UnitTestResult sweep_grass_battles(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.grass.every = 8;
    return sweep_variants(logger, row, FaultClass::GRASS_BATTLES, {row_variant(logger, "", "a battle each 8 grass tiles", row, std::move(knobs))});
}

//  Row `row` with every warp's fade-in slow, all through the row.
UnitTestResult sweep_slow_fade_in(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.slow_fade_in = true;
    return sweep_variants(logger, row, FaultClass::SLOW_FADE_IN, {row_variant(logger, "", "a slow fade-in after every warp", row, std::move(knobs))});
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

//  Row `row` with the scene of its step `c` starting on the end of `walk`,
//  the route before it: why the row fails or the walk fights the scene,
//  or nothing.
std::optional<std::string> scene_early_at(Logger& logger, const StoryCheckpoint& row, const StoryStep& walk){
    SimWorldKnobs knobs;
    knobs.scene_at_walk_end = [id = row.id, name = walk.name](const std::string& r, const std::string& s){
        return r == id && s == name;
    };
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
    const Pokemon::SegmentReport report = run_rows_on(executor, logger, {.from = row.id, .to = row.id});
    const SimRun& run = executor.result();
    if (!report.completed){
        return std::format("stopped in {}: {}", run.failed_step, run.error);
    }
    const std::string early = std::format("early scene after {}", walk.name);
    const bool fought = std::any_of(run.scenes_fought.begin(), run.scenes_fought.end(), [&](const std::string& scene){
        return scene.ends_with(early);
    });
    if (fought){
        return "the walk fought it as a trainer's";
    }
    return std::nullopt;
}

//  Row `row` with the scene of each scene step after a route starting on
//  that route's target, one at a time.
UnitTestResult sweep_scene_early(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
    for (size_t c = 1; c < row.steps.size(); c++){
        const std::optional<size_t> w = route_before_scene(row.steps, c);
        if (!w){
            continue;
        }
        const StoryStep& walk = row.steps[*w];
        variants.push_back({row.steps[c].name, std::format("starts on {}'s end", walk.name), [&logger, &row, &walk]{
            return scene_early_at(logger, row, walk);
        }});
    }
    return sweep_variants(logger, row, FaultClass::SCENE_EARLY, variants);
}

//  Class `fault` swept over row `row`.
UnitTestResult sweep_fault(Logger& logger, FaultClass fault, const StoryCheckpoint& row){
    switch (fault){
    case FaultClass::SCENE_END_POSE:
        return sweep_scene_end_pose(logger, row);
    case FaultClass::BATTLE_OPENING:
        return sweep_battle_opening(logger, row);
    case FaultClass::GRASS_BATTLES:
        return sweep_grass_battles(logger, row);
    case FaultClass::SCENE_EARLY:
        return sweep_scene_early(logger, row);
    case FaultClass::SLOW_FADE_IN:
        return sweep_slow_fade_in(logger, row);
    case FaultClass::LOST_TAP:
        break;
    }
    return true;
}

//  A fault class swept over row `row` (empty in FAULT_SWEEP_CASES: each
//  row from FIRST_ROW to LAST_ROW).
struct FaultSweepInput{
    FaultClass fault = FaultClass::SCENE_END_POSE;
    std::string row = {};
};
//  Whether a sweep is expected to fail is known_failing()'s, by name.
struct FaultSweepExpectation{
};
//  `name`: the fault class's in FAULT_SWEEP_CASES; "<row>, <fault class>"
//  in a test.
struct FaultSweepCase{
    std::string name;
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
        : UnitTest(std::format("PokemonBDSP::AutoStory - Fault sweep: {}", test_case.name))
        , m_fault(test_case.input.fault)
        , m_row(table_row(test_case.input.row))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        return known_failing().run(logger, name(), [&]{
            return Pokemon::sweep_clean_first(logger, m_row.id, run_row(logger, m_row.id, {}), [&]{
                return sweep_fault(logger, m_fault, m_row);
            });
        });
    }

private:
    FaultClass m_fault;
    const StoryCheckpoint& m_row;
};

} // namespace

const FaultSweepCase FAULT_SWEEP_CASES[] = {
    {.name = std::string(fault_class_name(FaultClass::SCENE_END_POSE)), .input = {.fault = FaultClass::SCENE_END_POSE}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::BATTLE_OPENING)), .input = {.fault = FaultClass::BATTLE_OPENING}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::GRASS_BATTLES)), .input = {.fault = FaultClass::GRASS_BATTLES}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::SCENE_EARLY)), .input = {.fault = FaultClass::SCENE_EARLY}, .expectation = {}},
    {.name = std::string(fault_class_name(FaultClass::SLOW_FADE_IN)), .input = {.fault = FaultClass::SLOW_FADE_IN}, .expectation = {}},
};

void add_tests_AutoStory_FaultSweep(UnitTestDatabase& database){
    for (const FaultSweepCase& item : FAULT_SWEEP_CASES){
        for (const StoryCheckpoint& row : story_checkpoints()){
            if (row.id < FIRST_ROW || row.id > LAST_ROW){
                continue;
            }
            database.add<Test_AutoStoryFaultSweep>(FaultSweepCase{
                .name = std::format("{}, {}", row.id, item.name),
                .input = {.fault = item.input.fault, .row = row.id},
                .expectation = item.expectation,
            });
        }
    }
}

} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
