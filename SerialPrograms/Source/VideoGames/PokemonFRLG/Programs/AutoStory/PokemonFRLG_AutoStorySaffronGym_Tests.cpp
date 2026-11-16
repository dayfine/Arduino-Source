/*  AutoStory Saffron Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 18.01, 18.02 and 18.02b (StoryData/frlg-18.02*.plan.json ...) on the
 *  overworld sim: Saffron Gym's teleport pads, from the door to the tile below
 *  Sabrina and back out to Saffron's Center, with no trainer met on the way;
 *  Sabrina's battle is a checkpoint of its own. Then 18.03, the Fly to
 *  Celadon's X items clerk and back to Saffron's Center. Last, that the
 *  walks through Saffron and in and out of its buildings (16.06, 16.06b,
 *  18.02b) stay near the straight line and never cross a tile twice.
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows18_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStorySaffronGymInput{
    std::function<UnitTestResult()> body;
};

struct AutoStorySaffronGymExpectation{
    std::string_view description;
};

struct AutoStorySaffronGymCase{
    std::string name;
    AutoStorySaffronGymInput input;
    AutoStorySaffronGymExpectation expectation;
};

class Test_AutoStorySaffronGym : public UnitTest{
public:
    explicit Test_AutoStorySaffronGym(AutoStorySaffronGymCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return m_case.input.body();
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    AutoStorySaffronGymCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Where each of `row`'s pad steps (the STEP steps, which are only ever
//  onto a pad here) leaves the player, in order, after running the row's
//  steps (a Fly among them) on the sim up to (not including) its Center
//  step, on the maps of its start flags. Fails with the row's message if a
//  step does not run.
struct PadWalk{
    std::string failure;
    std::vector<MapPosition> landings;
    MapPosition end;
    bool clean = false;
};

PadWalk walk_pads(const StoryCheckpoint& row){
    PadWalk walk;
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    const std::set<std::string> flags(row.start_flags.begin(), row.start_flags.end());
    OverworldSim sim(run_story_maps(flags), MapPosition{at.map, at.tile}, *at.facing);
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (step.kind == StoryStepKind::TALK || step.kind == StoryStepKind::BATTLE){
            continue;
        }
        walk.failure = row_step_on_sim(sim, at, step, row.id, flags);
        if (!walk.failure.empty()){
            break;
        }
        if (step.kind == StoryStepKind::STEP){
            walk.landings.push_back(sim.state().position);
        }
    }
    walk.end = sim.state().position;
    walk.clean = sim.state().trainers_beaten.empty() && sim.state().violations.empty();
    return walk;
}

//  The pads, in the order the way in takes them, and where each leaves the
//  player: (18, 20) to (28, 23), (20, 20) to (28, 4), (20, 7) to (0, 4),
//  (0, 7) to (18, 15) (SaffronCity_Gym map.json warps 3, 24, 22 and 5).
UnitTestResult test_pads_to_sabrina(){
    const PadWalk walk = walk_pads(checkpoint("18.01"));
    if (!walk.failure.empty()){
        return walk.failure;
    }
    const std::vector<MapPosition> expected{
        {"SaffronCity_Gym", {28, 23}}, {"SaffronCity_Gym", {28, 4}}, {"SaffronCity_Gym", {0, 4}}, {"SaffronCity_Gym", {18, 15}},
    };
    if (walk.landings != expected){
        return "18.01: the pads did not land on (28, 23), (28, 4), (0, 4) and (18, 15), in that order.";
    }
    const MapPosition below_sabrina{"SaffronCity_Gym", {14, 12}};
    if (!(walk.end == below_sabrina)){
        return std::format("18.01: ended on {}, not below Sabrina {}.", to_string(walk.end), to_string(below_sabrina));
    }
    if (!walk.clean){
        return "18.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  A boss battle is its own checkpoint (user, 2026-11-12): 18.02 ends with
//  Sabrina's battle, in front of her, and 18.02b starts there.
UnitTestResult test_sabrina_own_checkpoint(){
    const std::vector<StoryStep>& steps = checkpoint("18.02").steps;
    if (steps.empty() || steps.back().kind != StoryStepKind::BATTLE || steps.back().name != "sabrina"){
        return "18.02 does not end with Sabrina's battle.";
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("18.02b").start));
    const MapPosition below_sabrina{"SaffronCity_Gym", {14, 12}};
    if (next.map != below_sabrina.map || !(next.tile == below_sabrina.point) || next.facing != Direction::NORTH){
        return "18.02b doesn't start in front of Sabrina, where her battle leaves the player.";
    }
    return true;
}

//  The way out (18.02b): the same pads the other way, each pair linked both
//  ways, then the door onto Saffron City and the Fly to below the Center's door.
UnitTestResult test_pads_out_to_center(){
    const PadWalk walk = walk_pads(checkpoint("18.02b"));
    if (!walk.failure.empty()){
        return walk.failure;
    }
    const std::vector<MapPosition> expected{
        {"SaffronCity_Gym", {0, 7}}, {"SaffronCity_Gym", {20, 7}}, {"SaffronCity_Gym", {20, 20}}, {"SaffronCity_Gym", {18, 20}},
    };
    if (walk.landings != expected){
        return "18.02b: the pads did not land on (0, 7), (20, 7), (20, 20) and (18, 20), in that order.";
    }
    const MapPosition below_center{"SaffronCity", {24, 39}};
    if (!(walk.end == below_center)){
        return std::format("18.02b: ended on {}, not below Saffron's Center door {}.", to_string(walk.end), to_string(below_center));
    }
    if (!walk.clean){
        return "18.02b: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  The control for the two above: the sim does see Cameron (24, 18). Row 21,
//  where the planner would walk (20, 21) from (28, 23) if the row did not
//  stop on row 22 first, is in his line (24, 19 to 21), so a walk along it
//  is seen and fails.
UnitTestResult test_cameron_sees_row_21(){
    OverworldSim sim(story_maps(), MapPosition{"SaffronCity_Gym", {28, 21}}, Direction::WEST);
    StoryPose at{"SaffronCity_Gym", {28, 21}, Direction::WEST};
    StoryStep along;
    along.kind = StoryStepKind::ROUTE;
    along.name = "along_row_21";
    const Pokemon::GridPose target{"SaffronCity_Gym", {{20, 21}}, Direction::WEST};
    along.args = {{"target", target}, {"fight_wild", false}};
    const std::string failure = row_step_on_sim(sim, at, along, "18.01");
    if (failure.empty() && sim.state().trainers_beaten.empty() && sim.state().violations.empty()){
        return "A walk along row 21 was not seen by Cameron (24, 18).";
    }
    return true;
}

//  18.03 (user, 2026-11-15: 2 X Accuracies before the Route 22 rival) from
//  Saffron's nurse: out, the Fly to Celadon, 1F's lift to 5F, its X items
//  clerk, where the Mart step buys, the lift back, out of the store, the Fly
//  to Saffron and below the Center's door, where center_enter takes the
//  player to 17.02's start. Nobody met.
UnitTestResult test_celadon_x_accuracies(){
    const StoryCheckpoint& row = checkpoint("18.03");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    //  The Department Store's lift, as 10.01's: its room and its floors.
    SimScript script;
    script.lift_floors[std::string(DEPT_STORE_LIFT_MAP)] = department_store_lift_exits();
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing, script);
    const MapPosition clerk{"CeladonCity_DepartmentStore_5F", {3, 7}};
    bool bought_at_clerk = false;
    for (const StoryStep& step : row.steps){
        if (step.action == CENTER_ENTER_VARIANT){
            break;
        }
        if (step.action == MART_ACTION){
            bought_at_clerk = sim.state().position == clerk && at.facing == Direction::WEST;
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return std::format("18.03: ended on {}; {}", to_string(sim.state().position), failure);
        }
    }
    if (!bought_at_clerk){
        return std::format("18.03: the Mart step was not at the X items clerk {} facing west.", to_string(clerk));
    }
    const MapPosition below_center{"SaffronCity", {24, 39}};
    if (!(sim.state().position == below_center) || at.facing != Direction::NORTH){
        return std::format("18.03: ended on {}, not below Saffron's Center door {} facing north.", to_string(sim.state().position), to_string(below_center));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "18.03: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  One stretch of a row's walking on one map, between warps and the steps
//  that move the player without walking (a pad, Fly, an Escape Rope, the
//  Center's script): where it starts and the tiles it walks, in order.
struct Stretch{
    MapPosition from;
    std::vector<MapPosition> tiles;
};

struct RowStretches{
    std::string failure;
    std::vector<Stretch> stretches;
};

//  `route` walked from `at`, its tiles added to `out`'s stretches; `at`
//  moves to where it ends.
void add_route_tiles(const std::vector<MapData>& maps, const std::vector<RouteStep>& route, StoryPose& at, RowStretches& out){
    MapPosition position{at.map, at.tile};
    for (const RouteStep& leg : route){
        if (leg.action == RouteAction::WALK){
            const std::vector<MapPosition> tiles = leg_tiles(maps, position, leg);
            std::vector<MapPosition>& walked = out.stretches.back().tiles;
            walked.insert(walked.end(), tiles.empty() ? tiles.end() : tiles.begin() + 1, tiles.end());
        }
        if (leg.action == RouteAction::WALK && leg.warps){
            out.stretches.push_back({leg.end, {}});
        }
        position = leg.end;
        at.facing = leg.facing_after;
    }
    at.map = position.map;
    at.tile = position.point;
}

//  The maps, doors and rules a row's routes are planned on.
struct RowPlanning{
    const std::vector<MapData>& maps;
    Pokemon::GateState gates;
    Pokemon::GateRules rules;
};

//  `step` from `at`: a route's tiles added to `out`, or a pose the step
//  declares elsewhere starting a stretch; `at` moves to where it ends.
//  The failure, or empty.
std::string add_step_tiles(const RowPlanning& planning, const StoryStep& step, StoryPose& at, RowStretches& out){
    if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
        const std::optional<std::vector<RouteStep>> route = walked_route(at, step, planning.maps, planning.gates, planning.rules);
        if (!route){
            return std::format("{}: no route from {}.", step.name, to_string(MapPosition{at.map, at.tile}));
        }
        add_route_tiles(planning.maps, *route, at, out);
        return "";
    }
    if (!step.pose_after){
        return "";
    }
    const StoryPose to = *story_pose(Pokemon::pose_of(*step.pose_after));
    if (to.map != at.map || !(to.tile == at.tile)){
        out.stretches.push_back({MapPosition{to.map, to.tile}, {}});
    }
    at = to;
    return "";
}

//  The tiles `row`'s routes walk, planned as the program plans them from
//  the row's save (on the maps of its start flags), each from where the
//  step before leaves the player.
RowStretches row_stretches(const StoryCheckpoint& row){
    const std::set<std::string> flags(row.start_flags.begin(), row.start_flags.end());
    const RowPlanning planning{
        .maps = run_story_maps(flags),
        .gates = row.start_gates.value_or(Pokemon::GateState()),
        .rules = Pokemon::GateRules{.card_key_held = flags.contains("card_key_held")},
    };
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    RowStretches out;
    out.stretches.push_back({MapPosition{at.map, at.tile}, {}});
    for (const StoryStep& step : row.steps){
        if (const std::string failure = add_step_tiles(planning, step, at, out); !failure.empty()){
            out.failure = std::format("{}/{}", row.id, failure);
            return out;
        }
    }
    return out;
}

//  Tiles a map makes every walk across it take beyond the straight line.
struct ForcedDetour{
    std::string_view map;
    int tiles = 0;
};

//  A row whose walks should be direct, and the detours its maps force.
struct DirectRow{
    std::string_view id;
    std::vector<ForcedDetour> detours;
};

//  16.06 walks nothing: Escape Rope out of Silph Co., Fly to the Center.
const DirectRow DIRECT_16_06{"16.06", {}};
//  Saffron City from the Center's door (24, 39) to the Gym's (46, 12): the
//  only way north past row 19 is columns 50..52, 4 columns east of the
//  door and back (8 tiles), and row 39 is the Lass's walk (27..37, 39), a
//  row down and back (2 tiles). The fewest tiles there are 58.
const DirectRow DIRECT_16_06B{"16.06b", {{"SaffronCity", 10}}};
//  18.02b's pads, then only the entrance room to the Gym's door and the Fly.
const DirectRow DIRECT_18_02B{"18.02b", {}};

int straight_line(const TilePos& from, const TilePos& to){
    return std::abs(from.x - to.x) + std::abs(from.y - to.y);
}

//  Console B, 2026-11-15: 16.06 walked about 2 minutes down Silph Co. with
//  16 tiles walked again after re-plans; 18.02b walked about 80 tiles to
//  the Center for a straight line of 48, and 16.06b about 79. Each stretch
//  of `expected`'s walking, as planned, takes at most 1.2 times the
//  straight line from its start to its end (plus what the map forces), and
//  no tile is walked onto twice.
UnitTestResult test_direct_walks(const DirectRow& expected){
    const RowStretches walk = row_stretches(checkpoint(expected.id));
    if (!walk.failure.empty()){
        return walk.failure;
    }
    std::vector<MapPosition> crossed;
    for (const Stretch& stretch : walk.stretches){
        crossed.push_back(stretch.from);
        for (const MapPosition& tile : stretch.tiles){
            if (std::find(crossed.begin(), crossed.end(), tile) != crossed.end()){
                return std::format("{}: walks onto {} twice.", expected.id, to_string(tile));
            }
            crossed.push_back(tile);
        }
        if (stretch.tiles.empty()){
            continue;
        }
        int forced = 0;
        for (const ForcedDetour& detour : expected.detours){
            forced += detour.map == stretch.from.map ? detour.tiles : 0;
        }
        const int line = straight_line(stretch.from.point, stretch.tiles.back().point) + forced;
        const int tiles = (int)stretch.tiles.size();
        if (tiles * 5 > line * 6){
            return std::format(
                "{}: walks {} tiles from {} to {}, more than 1.2 times {} (the straight line, plus {} the map forces).",
                expected.id, tiles, to_string(stretch.from), to_string(stretch.tiles.back()), line, forced
            );
        }
    }
    return true;
}

const AutoStorySaffronGymCase AUTO_STORY_SAFFRON_GYM_CASES[] = {
    {
        .name = "18.01 Saffron Gym's pads from the door to below Sabrina",
        .input = {.body = test_pads_to_sabrina},
        .expectation = {.description = "four pads, landing in the rooms in order, below Sabrina, nobody met"},
    },
    {
        .name = "18.02 Sabrina's battle is its own checkpoint",
        .input = {.body = test_sabrina_own_checkpoint},
        .expectation = {.description = "18.02 ends with her battle; 18.02b starts in front of her"},
    },
    {
        .name = "18.02b Saffron Gym's pads back out to the Center's door",
        .input = {.body = test_pads_out_to_center},
        .expectation = {.description = "four pads the other way, out of the door, the Fly to below Saffron's Center door, nobody met"},
    },
    {
        .name = "Saffron Gym: Cameron sees a walk along row 21",
        .input = {.body = test_cameron_sees_row_21},
        .expectation = {.description = "the sim meets a trainer on row 21, so the rows' row 22 is a real detour"},
    },
    {
        .name = "18.03 Fly to Celadon's X items clerk and back to Saffron's Center",
        .input = {.body = test_celadon_x_accuracies},
        .expectation = {.description = "the Mart step at 5F's clerk facing west, then below Saffron's Center door facing north, nobody met"},
    },
    {
        .name = "16.06 walks direct: Escape Rope and Fly",
        .input = {.body = []{ return test_direct_walks(DIRECT_16_06); }},
        .expectation = {.description = "no walk longer than 1.2 times its straight line, no tile twice"},
    },
    {
        .name = "16.06b walks direct: Saffron's Center to the Gym's door",
        .input = {.body = []{ return test_direct_walks(DIRECT_16_06B); }},
        .expectation = {.description = "62 tiles on Saffron City, within 1.2 times 58 (48 straight, 10 forced), no tile twice"},
    },
    {
        .name = "18.02b walks direct: the pads, the Gym's door, Fly",
        .input = {.body = []{ return test_direct_walks(DIRECT_18_02B); }},
        .expectation = {.description = "each room's walk its straight line, no tile twice"},
    },
};

} // namespace


void add_tests_AutoStorySaffronGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_SAFFRON_GYM_CASES){
        database.add<Test_AutoStorySaffronGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
