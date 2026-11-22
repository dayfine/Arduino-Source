/*  BDSP AutoStory - Fault sweeps on the story simulation
 *
 *  From: https://github.com/PokemonAutomation/
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
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/Navigation/Grid/Maps/Pokemon_MapData.h"
#include "Pokemon/Navigation/Simulation/Pokemon_KnownFailing.h"
#include "Pokemon/Navigation/Simulation/Pokemon_SimSweep.h"
#include "PokemonBDSP/Programs/AutoStory/Actions/PokemonBDSP_AutoStory_Events.h"
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
using Pokemon::GridPose;
using Pokemon::SweepVariant;
using Pokemon::TilePos;

enum class FaultClass{
    SCENE_END_POSE,
    BATTLE_OPENING,
    GRASS_BATTLES,
    SCENE_EARLY,
    SLOW_FADE_IN,
    LATE_FADE,
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
    case FaultClass::LATE_FADE:
        return "late fades at a Pokemon Center's doors";
    case FaultClass::LOST_TAP:
        return "lost tap";
    }
    return "?";
}

constexpr std::string_view FIRST_ROW = "00.00";
constexpr std::string_view LAST_ROW = "01.11";

const Pokemon::KnownFailing& known_failing(){
    static const Pokemon::KnownFailing list({});
    return list;
}

const std::map<std::string, std::string> PROVEN_SCENE_ENDS{
    {"rowan_and_dawn", "lab-B 2026-11-09, 7 of 7 runs on (6, 8)"},
};

std::optional<std::string> run_row(Logger& logger, const std::string& row, SimWorldKnobs knobs){
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
    const SimRunReport report = run_rows_on(executor, logger, {.from = row, .to = row});
    if (report.completed){
        return std::nullopt;
    }
    return std::format("stopped in {}: {}", executor.result().failed_step, executor.result().error);
}

bool game_sets_end(std::string_view action){
    return action == "new_game" || action == "talk" || action == "answer" ||
        action == "scene" || action == "battle" ||
        (action != "sandgem_potions" && story_events().contains(std::string(action)));
}

bool walks(std::string_view action){
    return action == "route" || action == "legs" || action == "step";
}

bool already_there(Logger& logger, const StoryCheckpoint& row, size_t c, const GridPose& pose){
    StoryCheckpoint head = row;
    head.steps.resize(c);
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, {});
    if (!run_rows_on(executor, logger, {.from = row.id, .to = row.id}, {head}).completed || executor.result().world == nullptr){
        return false;
    }
    const SimRun& run = executor.result();
    for (const TilePos& tile : pose.tiles){
        try{
            if (story_to_world(*run.world, pose.map, tile) == run.end){
                return true;
            }
        }catch (const InternalProgramError&){}
    }
    return false;
}

bool wild_or_trainers(const BdspStep& step){
    std::string map;
    if (step.pose_after){
        map = step.pose_after->map;
    }else if (step.action == "route"){
        map = std::get<RouteStepArgs>(step.args).target.map;
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

void walkable_neighbors(const StoryWorld* world, const GridPose& pose, const TilePos& center, std::set<std::pair<int, int>>& seen, std::vector<TilePos>& out){
    const auto step_dir = [](const TilePos& tile, Direction direction){
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
        const TilePos one = step_dir(center, first);
        add(one);
        for (Direction second : all){
            const TilePos two = step_dir(one, second);
            const bool diagonal = two.x != center.x && two.y != center.y;
            if (diagonal && can_step(one, second)){
                add(two);
            }
        }
    }
}

std::vector<TilePos> walkable_neighbors(const GridPose& pose){
    const StoryWorld* world = pose.map.empty() ? nullptr : story_plan_world(pose.map, pose.map);
    std::vector<TilePos> out;
    if (world == nullptr){
        return out;
    }
    std::set<std::pair<int, int>> seen;
    for (const TilePos& tile : pose.tiles){
        seen.insert({tile.x, tile.y});
    }
    for (const TilePos& tile : pose.tiles){
        walkable_neighbors(world, pose, tile, seen, out);
    }
    return out;
}

UnitTestResult sweep_variants(Logger& logger, const StoryCheckpoint& row, FaultClass fault, const std::vector<SweepVariant>& variants){
    return Pokemon::run_sweep(logger, row.id, fault_class_name(fault), variants);
}

SweepVariant row_variant(Logger& logger, std::string step, std::string label, const StoryCheckpoint& row, SimWorldKnobs knobs){
    return {std::move(step), std::move(label), [&logger, &row, knobs = std::move(knobs)]{
        return run_row(logger, row.id, knobs);
    }};
}

SweepVariant end_pose_variant(Logger& logger, const StoryCheckpoint& row, size_t c, const TilePos& tile){
    const BdspStep& step = row.steps[c];
    SimWorldKnobs knobs;
    knobs.lands[step.name] = GridPose{.map = step.pose_after->map, .tiles = {tile}, .facing = step.pose_after->facing};
    return row_variant(logger, step.name, std::format("ends on ({}, {})", tile.x, tile.y), row, std::move(knobs));
}

UnitTestResult sweep_scene_end_pose(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
    for (size_t c = 0; c < row.steps.size(); c++){
        const BdspStep& step = row.steps[c];
        if (!game_sets_end(step.action) || !step.pose_after || PROVEN_SCENE_ENDS.contains(step.name) ||
            already_there(logger, row, c, *step.pose_after)){
            continue;
        }
        for (const TilePos& tile : walkable_neighbors(*step.pose_after)){
            variants.push_back(end_pose_variant(logger, row, c, tile));
        }
    }
    return sweep_variants(logger, row, FaultClass::SCENE_END_POSE, variants);
}

UnitTestResult sweep_battle_opening(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
    for (size_t c = 0; c < row.steps.size(); c++){
        const BdspStep& step = row.steps[c];
        const bool after_walk = c > 0 && walks(row.steps[c - 1].action) && wild_or_trainers(row.steps[c - 1]);
        if (step.action != "battle" && !after_walk){
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

UnitTestResult sweep_grass_battles(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.grass.every = 8;
    return sweep_variants(logger, row, FaultClass::GRASS_BATTLES, {row_variant(logger, "", "a battle each 8 grass tiles", row, std::move(knobs))});
}

constexpr Milliseconds SLOW_CENTER_FADE_IN = Milliseconds(4000);

UnitTestResult sweep_slow_fade_in(Logger& logger, const StoryCheckpoint& row){
    SimWorldKnobs knobs;
    knobs.slow_fade_in = true;
    for (const BdspStep& step : row.steps){
        if (step.action == "heal"){
            knobs.centers[step.name] = {.fade_in = SLOW_CENTER_FADE_IN};
        }
    }
    return sweep_variants(logger, row, FaultClass::SLOW_FADE_IN, {row_variant(logger, "", "a slow fade-in after every warp", row, std::move(knobs))});
}

UnitTestResult sweep_late_fade(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
    for (const BdspStep& step : row.steps){
        if (step.action != "heal"){
            continue;
        }
        SimWorldKnobs knobs;
        knobs.centers[step.name] = {.late_fade = true};
        variants.push_back(row_variant(logger, step.name, "late fades at both doors", row, std::move(knobs)));
    }
    return sweep_variants(logger, row, FaultClass::LATE_FADE, variants);
}

std::optional<size_t> route_before_scene(const std::vector<BdspStep>& steps, size_t c){
    if (steps[c].action != "scene" || c == 0){
        return std::nullopt;
    }
    if (steps[c - 1].action == "route"){
        return c - 1;
    }
    const BdspStep& step = steps[c - 1];
    const bool onto_trigger = step.action == "step" && std::get<StepMoveArgs>(step.args).variant.empty() && !step.pose_after;
    if (onto_trigger && c >= 2 && steps[c - 2].action == "route"){
        return c - 2;
    }
    return std::nullopt;
}

std::optional<std::string> scene_early_at(Logger& logger, const StoryCheckpoint& row, const BdspStep& walk){
    SimWorldKnobs knobs;
    knobs.scene_at_walk_end = [id = row.id, name = walk.name](const std::string& r, const std::string& s){
        return r == id && s == name;
    };
    SimExecutor executor(logger, {.fade = {}, .banner = {}}, std::move(knobs));
    const SimRunReport report = run_rows_on(executor, logger, {.from = row.id, .to = row.id});
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

UnitTestResult sweep_scene_early(Logger& logger, const StoryCheckpoint& row){
    std::vector<SweepVariant> variants;
    for (size_t c = 1; c < row.steps.size(); c++){
        const std::optional<size_t> w = route_before_scene(row.steps, c);
        if (!w){
            continue;
        }
        const BdspStep& walk = row.steps[*w];
        variants.push_back({row.steps[c].name, std::format("starts on {}'s end", walk.name), [&logger, &row, &walk]{
            return scene_early_at(logger, row, walk);
        }});
    }
    return sweep_variants(logger, row, FaultClass::SCENE_EARLY, variants);
}

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
    case FaultClass::LATE_FADE:
        return sweep_late_fade(logger, row);
    case FaultClass::LOST_TAP:
        break;
    }
    return true;
}

struct FaultSweepCase{
    struct Input{
        FaultClass fault = FaultClass::SCENE_END_POSE;
        std::string row = {};
    };
    struct Expectation{
    };
    std::string name;
    Input input;
    Expectation expectation;
};

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
    {.name = std::string(fault_class_name(FaultClass::LATE_FADE)), .input = {.fault = FaultClass::LATE_FADE}, .expectation = {}},
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
