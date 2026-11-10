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
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_Maps.h"
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
    //  Not swept yet: the measured movement table's lost taps (#293).
    LOST_TAP,
};
const char* fault_class_name(FaultClass fault){
    switch (fault){
    case FaultClass::SCENE_END_POSE:
        return "scene end pose";
    case FaultClass::BATTLE_OPENING:
        return "battle opening at step start";
    case FaultClass::GRASS_BATTLES:
        return "wild battles on every grass pass";
    case FaultClass::LOST_TAP:
        return "lost tap";
    }
    return "?";
}

//  The rows swept: those the clean simulation runs (SimFailureCases_Tests).
const char* FIRST_ROW = "00.00";
const char* LAST_ROW = "01.11";

//  Sweeps the program fails today, by test name, with the likely fix.
const std::map<std::string, std::string> KNOWN_FAILING{
    {"PokemonBDSP::AutoStory - Fault sweep: 01.06, wild battles on every grass pass",
        "below_rowans_trigger's open leg west 57: a battle in its grass leaves it on Jubilife's (44, 22) or (45, 22), "
        "from which the Ace Trainer's (45..47, 21) leaves no plan: end the long leg on a column both of whose candidates clip north to row 8"},
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
    const std::optional<Pose>& end = step.pose_after ? step.pose_after : step.target;
    if (!end){
        return true;
    }
    for (const char* place : {"Route", "Gate", "Mine", "RavagedPath"}){
        if (end->map.find(place) != std::string::npos){
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
        return sim_can_step(sim, direction);
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
        if (!game_sets_end(step.kind) || !step.pose_after || PROVEN_SCENE_ENDS.contains(step.name) ||
            already_there(logger, row, c, *step.pose_after)){
            continue;
        }
        for (const TilePos& tile : walkable_neighbors(*step.pose_after)){
            SimWorldKnobs knobs;
            knobs.lands[step.name] = Pokemon::tile_pose(step.pose_after->map, {tile}, Pokemon::facing_of(*step.pose_after));
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
        case FaultClass::LOST_TAP:
            break;
        }
        return true;
    }

    FaultClass m_fault;
    const StoryCheckpoint& m_row;
};

} // namespace

const FaultSweepCase FAULT_SWEEP_CASES[] = {
    {.input = {.fault = FaultClass::SCENE_END_POSE}, .expectation = {}},
    {.input = {.fault = FaultClass::BATTLE_OPENING}, .expectation = {}},
    {.input = {.fault = FaultClass::GRASS_BATTLES}, .expectation = {}},
};

void add_tests_AutoStory_FaultSweep(UnitTestDatabase& database){
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
