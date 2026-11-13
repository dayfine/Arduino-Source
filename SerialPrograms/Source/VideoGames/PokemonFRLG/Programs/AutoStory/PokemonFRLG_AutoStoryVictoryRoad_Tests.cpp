/*  AutoStory Victory Road Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 19.01a and after (StoryData/frlg-19.0N.plan.json) on the overworld
 *  sim: Viridian's Center to Route 22's rival, Route 23, Victory Road and
 *  Indigo Plateau's Center.
 *
 */

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryPostconditionCheck.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFieldMoves.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows19_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTeachPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryVictoryRoadInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryVictoryRoadExpectation{
    std::string_view description;
};

struct AutoStoryVictoryRoadCase{
    AutoStoryVictoryRoadInput input;
    AutoStoryVictoryRoadExpectation expectation;
};

class Test_AutoStoryVictoryRoad : public UnitTest{
public:
    explicit Test_AutoStoryVictoryRoad(AutoStoryVictoryRoadCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.input.name))
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
    AutoStoryVictoryRoadCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Runs the walking steps of `row` on the sim from its start, up to (not
//  including) the step named `until`, or all of them if `until` is empty;
//  the steps that are not walks (the Mart, the level check, a scene, the
//  battle) are the console's. `marked` is where the sim stood after the
//  step named `mark`.
std::string walk_row(
    OverworldSim& sim, StoryPose& at, const StoryCheckpoint& row,
    std::string_view until, std::string_view mark = {}, MapPosition* marked = nullptr
){
    for (const StoryStep& step : row.steps){
        if (step.name == until){
            return "";
        }
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS && step.kind != StoryStepKind::STEP){
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
        if (marked != nullptr && step.name == mark){
            *marked = sim.state().position;
        }
    }
    return until.empty() ? "" : std::format("{}: no step {}.", row.id, until);
}

//  19.01a: the street to the Mart's counter, where 19.01 starts. 19.01: out
//  of the Mart, along the street to Route 22 and along it to the rival's
//  trigger column, then the step onto it. The rival is the battle step's, so
//  the walk ends on the trigger tile (33, 5), facing west, with no trainer met.
UnitTestResult test_route22_rival_trigger(){
    const StoryCheckpoint& shop = checkpoint("19.01a");
    StoryPose at = *story_pose(Pokemon::pose_of(*shop.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_row(sim, at, shop, ""); !failure.empty()){
        return failure;
    }
    const MapPosition counter{"ViridianCity_Mart", {4, 3}};
    if (!(sim.state().position == counter)){
        return std::format("19.01a: ended on {}, not the Mart's counter {}.", to_string(sim.state().position), to_string(counter));
    }
    const StoryCheckpoint& row = checkpoint("19.01");
    const StoryPose next = *story_pose(Pokemon::pose_of(*row.start));
    if (next.map != counter.map || !(next.tile == counter.point)){
        return "19.01 doesn't start where 19.01a ends.";
    }
    if (const std::string failure = walk_row(sim, at, row, "rival_route22"); !failure.empty()){
        return failure;
    }
    const MapPosition trigger{"Route22", {33, 5}};
    if (!(sim.state().position == trigger)){
        return std::format("19.01: ended on {}, not the rival's trigger {}.", to_string(sim.state().position), to_string(trigger));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "19.01: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  19.02: Route 22's ledge and the way round to the north gate; the step
//  onto the guard's trigger tile (7, 2) must land on it (the scene after it
//  is the console's), and the door beyond leads to Route 23's south end.
UnitTestResult test_gate_guard_trigger_and_route23(){
    const StoryCheckpoint& row = checkpoint("19.02");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    MapPosition at_trigger;
    if (const std::string failure = walk_row(sim, at, row, "", "gate_guard_trigger", &at_trigger); !failure.empty()){
        return failure;
    }
    const MapPosition trigger{"Route22_NorthEntrance", {7, 2}};
    if (!(at_trigger == trigger)){
        return std::format("19.02: the guard's step ended on {}, not the trigger {}.", to_string(at_trigger), to_string(trigger));
    }
    const MapPosition route23{"Route23", {8, 153}};
    if (!(sim.state().position == route23)){
        return std::format("19.02: ended on {}, not Route 23's south end {}.", to_string(sim.state().position), to_string(route23));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "19.02: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  The step of row `id` named `name`.
const StoryStep& step_of(std::string_view id, std::string_view name){
    for (const StoryStep& step : checkpoint(id).steps){
        if (step.name == name){
            return step;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No step {} in {}.", name, id));
}

//  19.01's rival rows are PokemonSimulation's planning_risk() for each foe
//  leading against the entry state of its route22-rival case
//  (src/pokesim/games/frlg/battle/player_strategy/planning.py): speed, the
//  80th percentile loss, the mean, the worst case, and the worst normal hit.
UnitTestResult test_rival_foe_threats_are_the_generators(){
    const StoryStep& rival = step_of("19.01", "rival_route22");
    const auto found = rival.args.find("foe_threats");
    const auto* rows = found == rival.args.end() ? nullptr : std::get_if<std::vector<std::vector<int64_t>>>(&found->second);
    const std::vector<std::vector<int64_t>> expected = {
        {99, 29, 27, 60, 30, 0, 0},     //  Pidgeot
        {35, 46, 30, 126, 59, 0, 0},    //  Rhyhorn
        {86, 15, 4, 36, 18, 0, 0},      //  Gyarados
        {67, 0, 1, 50, 25, 0, 0},       //  Growlithe
        {126, 55, 20, 120, 60, 0, 0},   //  Alakazam
        {105, 69, 20, 156, 78, 0, 0},   //  Venusaur
    };
    if (rows == nullptr || *rows != expected){
        return "19.01 rival_route22: foe_threats are not the generator's rows for the route22-rival case.";
    }
    return true;
}

//  Where the sim stood after each walking step of `row`, by step name.
std::string walk_row_landings(
    OverworldSim& sim, StoryPose& at, const StoryCheckpoint& row,
    std::map<std::string, MapPosition>& landings
){
    for (const StoryStep& step : row.steps){
        if (step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS && step.kind != StoryStepKind::STEP && !is_field_move_step(step)){
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            return failure;
        }
        landings[step.name] = sim.state().position;
    }
    return "";
}

//  19.03: each badge guard's trigger step lands on the tile of its coord
//  event (Route23 map.json), and the row ends below Victory Road's mouth.
UnitTestResult test_route23_guard_triggers(){
    const StoryCheckpoint& row = checkpoint("19.03");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::map<std::string, MapPosition> landings;
    if (const std::string failure = walk_row_landings(sim, at, row, landings); !failure.empty()){
        return failure;
    }
    const std::pair<const char*, MapPosition> guards[] = {
        {"cascade_guard_trigger", {"Route23", {13, 149}}},
        {"thunder_guard_trigger", {"Route23", {7, 140}}},
        {"rainbow_guard_trigger", {"Route23", {12, 123}}},
        {"soul_guard_trigger", {"Route23", {13, 112}}},
        {"marsh_guard_trigger", {"Route23", {9, 94}}},
        {"volcano_guard_trigger", {"Route23", {12, 61}}},
        {"earth_guard_trigger", {"Route23", {5, 35}}},
    };
    for (const auto& guard : guards){
        const MapPosition& landed = landings[guard.first];
        if (!(landed == guard.second)){
            return std::format("19.03 {}: landed on {}, not {}.", guard.first, to_string(landed), to_string(guard.second));
        }
    }
    const MapPosition mouth{"Route23", {5, 29}};
    if (!(sim.state().position == mouth)){
        return std::format("19.03: ended on {}, not {}.", to_string(sim.state().position), to_string(mouth));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return "19.03: met a trainer or broke a rule on the way.";
    }
    return true;
}

//  19.04: Strength replaces Water Pulse, as pokeguy keeps Strength, Surf,
//  Bite and Blizzard; the row ends on Victory Road 1F's south end.
UnitTestResult test_strength_teach_and_cave_mouth(){
    const StoryStep& teach = step_of("19.04", "teach_strength");
    const std::optional<TeachPlan> plan = teach_plan_of(teach.args);
    if (!plan.has_value() || plan->item != "hm04" || plan->move != "strength"){
        return "19.04 teach_strength: not HM04 Strength.";
    }
    const std::vector<std::string> keep = {"surf", "bite", "blizzard"};
    if (plan->keep != keep){
        return "19.04 teach_strength: keep is not surf, bite, blizzard.";
    }
    const StoryCheckpoint& row = checkpoint("19.04");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_row(sim, at, row, ""); !failure.empty()){
        return failure;
    }
    const MapPosition end{"VictoryRoad_1F", {11, 20}};
    if (!(sim.state().position == end)){
        return std::format("19.04: ended on {}, not {}.", to_string(sim.state().position), to_string(end));
    }
    return true;
}

//  A Victory Road floor's row on the sim from its start_gates, its boulder
//  pushes included: it starts where `previous` ends, turns on the switch
//  `switch_key`, meets no trainer, breaks no rule and ends on `end`.
UnitTestResult cave_floor(std::string_view previous, std::string_view id, std::string_view switch_key, const MapPosition& end){
    const StoryCheckpoint& before = checkpoint(previous);
    const StoryCheckpoint& row = checkpoint(id);
    const StoryPose at_start = *story_pose(Pokemon::pose_of(*row.start));
    const std::optional<Pokemon::GridPose> before_pose = Pokemon::checkpoint_end_pose(before);
    const std::optional<StoryPose> before_end = before_pose ? story_pose(Pokemon::pose_of(*before_pose)) : std::nullopt;
    if (!before_end || before_end->map != at_start.map || !(before_end->tile == at_start.tile)){
        return std::format("{} doesn't start where {} ends.", id, previous);
    }
    StoryPose at = at_start;
    SimScript script;
    script.gates = row.start_gates.value_or(Pokemon::GateState());
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, at.facing.value_or(Direction::NORTH), script);
    for (const StoryStep& step : row.steps){
        const bool push = step.kind == StoryStepKind::GAME && step.action == PUSH_BOULDER_VARIANT;
        if (!push && step.kind != StoryStepKind::ROUTE && step.kind != StoryStepKind::LEGS && step.kind != StoryStepKind::STEP){
            continue;
        }
        if (const std::string failure = row_step_on_sim(sim, at, step, row.id); !failure.empty()){
            std::string log; for (size_t c = sim.state().log.size() > 25 ? sim.state().log.size() - 25 : 0; c < sim.state().log.size(); c++){ log += "\n  " + sim.state().log[c]; } return std::format("{} {}: ended on {}; {}{}", id, step.name, to_string(sim.state().position), failure, log);
        }
        if (!sim.state().violations.empty()){
            return std::format("{} {}: broke a rule on {}: {}", id, step.name, to_string(sim.state().position), sim.state().violations.front());
        }
        if (!sim.state().trainers_beaten.empty()){
            return std::format("{} {}: met a trainer: {}", id, step.name, sim.state().trainers_beaten.front());
        }
    }
    if (!sim.state().gates.switch_on(std::string(switch_key))){
        return std::format("{}: the switch {} is not on.", id, switch_key);
    }
    if (!(sim.state().position == end)){
        return std::format("{}: ended on {}, not {}.", id, to_string(sim.state().position), to_string(end));
    }
    if (!sim.state().trainers_beaten.empty() || !sim.state().violations.empty()){
        return std::format("{}: met a trainer or broke a rule on the way.", id);
    }
    return true;
}

//  19.04b: 1F's boulder from (7, 18) onto the switch (20, 16), the barrier
//  open, up the ladder to 2F.
UnitTestResult test_victory_road_1f(){
    return cave_floor("19.04", "19.04b", "VictoryRoad_1F/floor_switch_1", MapPosition{"VictoryRoad_2F", {1, 9}});
}

//  19.04c: 2F's west boulder onto the switch (2, 19), then up to 3F and its
//  north boulder onto the switch (7, 7).
UnitTestResult test_victory_road_2f_3f(){
    return cave_floor("19.04b", "19.04c", "VictoryRoad_3F/floor_switch_1", MapPosition{"VictoryRoad_3F", {6, 7}});
}

//  19.04d: 3F's boulder down the hole, after it to 2F, onto 2F's second
//  switch (14, 19), then the exit to Route 23, a tile below its mouth (18, 29), where 19.05 starts.
UnitTestResult test_victory_road_hole_and_exit(){
    return cave_floor("19.04c", "19.04d", "VictoryRoad_2F/floor_switch_2", MapPosition{"Route23", {18, 29}});
}

//  19.05: from Victory Road's exit on Route 23 to the nurse's counter.
UnitTestResult test_indigo_center_walk(){
    const StoryCheckpoint& row = checkpoint("19.05");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    if (const std::string failure = walk_row(sim, at, row, ""); !failure.empty()){
        return failure;
    }
    const MapPosition end{"IndigoPlateau_PokemonCenter_1F", {13, 12}};
    if (!(sim.state().position == end)){
        return std::format("19.05: ended on {}, not {}.", to_string(sim.state().position), to_string(end));
    }
    return true;
}

const AutoStoryVictoryRoadCase AUTO_STORY_VICTORY_ROAD_CASES[] = {
    {
        .input = {.name = "19.01a Viridian's Mart and 19.01 to the Route 22 rival's trigger", .body = test_route22_rival_trigger},
        .expectation = {.description = "Route 22 (33, 5), no trainer met"},
    },
    {
        .input = {.name = "19.02 Route 22's gate guard trigger, onto Route 23", .body = test_gate_guard_trigger_and_route23},
        .expectation = {.description = "the trigger (7, 2) stepped onto, Route 23 (8, 153), no trainer met"},
    },
    {
        .input = {.name = "19.01 the rival's foe rows are the generator's", .body = test_rival_foe_threats_are_the_generators},
        .expectation = {.description = "PokemonSimulation planning_risk() rows"},
    },
    {
        .input = {.name = "19.03 Route 23's guard triggers, to Victory Road's mouth", .body = test_route23_guard_triggers},
        .expectation = {.description = "each trigger tile landed on, Route 23 (5, 29)"},
    },
    {
        .input = {.name = "19.04 Strength over Water Pulse, into the cave", .body = test_strength_teach_and_cave_mouth},
        .expectation = {.description = "keep surf, bite, blizzard; Victory Road 1F (11, 20)"},
    },
    {
        .input = {.name = "19.04b Victory Road 1F's boulder switch, up to 2F", .body = test_victory_road_1f},
        .expectation = {.description = "1F's switch on, 2F (1, 9), no trainer met"},
    },
    {
        .input = {.name = "19.04c Victory Road 2F's and 3F's boulder switches", .body = test_victory_road_2f_3f},
        .expectation = {.description = "3F's switch on, 3F (6, 7), no trainer met"},
    },
    {
        .input = {.name = "19.04d Victory Road 3F's hole, 2F's second switch, out to Route 23", .body = test_victory_road_hole_and_exit},
        .expectation = {.description = "2F's east switch on, Route 23 (18, 29), no trainer met"},
    },
    {
        .input = {.name = "19.05 Indigo Plateau's Center", .body = test_indigo_center_walk},
        .expectation = {.description = "Indigo Plateau Center (13, 12)"},
    },
};

} // namespace


void add_tests_AutoStoryVictoryRoad(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VICTORY_ROAD_CASES){
        database.add<Test_AutoStoryVictoryRoad>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
