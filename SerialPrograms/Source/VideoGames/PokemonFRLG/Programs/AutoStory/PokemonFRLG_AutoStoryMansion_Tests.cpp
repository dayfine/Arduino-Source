/*  AutoStory Mansion Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 13.01 to 13.04 (StoryData/frlg-13.0N.plan.json) on the overworld
 *  sim: Cinnabar's Center to the Pokemon Mansion's statue switches, the
 *  SECRET KEY and the Escape Rope out. The sim has the map's switch state
 *  (flag clear), so a step through a barrier the switches open is declared.
 *
 */

#include <format>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/Navigation/Pokemon_MapData.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalkTimes.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows13_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryMansionInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryMansionExpectation{
    std::string_view description;
};

struct AutoStoryMansionCase{
    AutoStoryMansionInput input;
    AutoStoryMansionExpectation expectation;
};

class Test_AutoStoryMansion : public UnitTest{
public:
    explicit Test_AutoStoryMansion(AutoStoryMansionCase test_case)
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
    AutoStoryMansionCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  Burglar Lewis, B1F's wanderer: he walks (19..21, 24..26) with sight 1 and
//  looks every way, so the sim, which takes his whole range as seen, meets
//  him where 13.03 passes (22, 24). Whether he is there is up to the dice;
//  the battle code fights him if he sees the player. Nobody else is met.
const std::string LEWIS = "poke_maniac";

struct MansionRow{
    std::string_view id;
    MapPosition end;
    std::vector<std::string> floors;
};

//  A step over a tile the switches open: the pack's maps are the clear
//  state, where it is a wall, so the sim cannot walk it and the step's
//  declared pose stands. `wall` is whether the tile it lands on is such a
//  barrier on the map; the step after it, onto floor, is declared too.
struct SwitchStep{
    std::string_view row;
    std::string_view step;
    bool wall;
};

const SwitchStep SWITCH_STEPS[] = {
    {"13.02", "into_opening_1", true},
    {"13.02", "into_opening_2", true},
    {"13.02", "through_opening", false},
    {"13.03", "into_room_2", true},
    {"13.03", "into_room_3", true},
    {"13.04", "into_opening", true},
    {"13.04", "through_opening", false},
};

const SwitchStep* switch_step(std::string_view row, std::string_view step){
    for (const SwitchStep& item : SWITCH_STEPS){
        if (item.row == row && item.step == step){
            return &item;
        }
    }
    return nullptr;
}

bool map_wall(const MapPosition& at){
    for (const MapData& map : story_maps()){
        if (map.name == at.map){
            return map.tiles.at(at.point) == TileKind::WALL;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No map {}.", at.map));
}

//  A row on the sim from its start: every step lands (a switch step on its
//  declared pose, on a barrier that is a wall on the map), the row ends on
//  its postcondition's tile after the floors it should cross, and nobody but Lewis is met and no rule is broken on the way (checked
//  after each step).
UnitTestResult test_row(const MansionRow& expected){
    const StoryCheckpoint& row = checkpoint(expected.id);
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::optional<OverworldSim> sim;
    sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> floors;
    for (const StoryStep& step : row.steps){
        if (const SwitchStep* barrier = switch_step(row.id, step.name)){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            if (map_wall(MapPosition{at.map, at.tile}) != barrier->wall){
                return std::format("{}/{}: expected {} on {}.", row.id, step.name, barrier->wall ? "a barrier" : "floor", to_string(MapPosition{at.map, at.tile}));
            }
            sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
        }else if (step.name == "escape_rope"){
            //  The rope's script has no map: the player stands where it says.
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
        }else if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("{}: ended on {}; {}", row.id, to_string(sim->state().position), failure);
        }
        if (step.name == "down_the_hole"){
            //  The hole's fade: a declared step does not wait it out, a walk does.
            sim->watch({}, OverworldWalkSteps::AFTER_WARP);
        }
        if (!sim->state().violations.empty()){
            return std::format("{}/{}: broke a rule on the way, on {}: {}.", row.id, step.name, to_string(sim->state().position), sim->state().violations.front());
        }
        for (const std::string& trainer : sim->state().trainers_beaten){
            if (trainer != LEWIS){
                return std::format("{}/{}: met a trainer: {}.", row.id, step.name, trainer);
            }
        }
        if (floors.empty() || floors.back() != at.map){
            floors.push_back(at.map);
        }
    }
    if (!(MapPosition{at.map, at.tile} == expected.end)){
        return std::format("{}: ended on {}, not {}.", row.id, to_string(MapPosition{at.map, at.tile}), to_string(expected.end));
    }
    if (floors != expected.floors){
        std::string crossed;
        for (const std::string& floor : floors){
            crossed += std::format(" {}", floor);
        }
        return std::format("{}: the steps crossed the wrong floors:{}.", row.id, crossed);
    }
    return true;
}

const MansionRow ROW_13_01{"13.01", {"PokemonMansion_3F", {12, 6}},
    {"PokemonMansion_3F"}};
const MansionRow ROW_13_02{"13.02", {"PokemonMansion_B1F", {34, 29}},
    {"PokemonMansion_3F", "PokemonMansion_1F", "PokemonMansion_B1F"}};
const MansionRow ROW_13_03{"13.03", {"PokemonMansion_B1F", {27, 6}},
    {"PokemonMansion_B1F"}};
const MansionRow ROW_13_04{"13.04", {"CinnabarIsland", {8, 4}},
    {"PokemonMansion_B1F", "CinnabarIsland"}};

const AutoStoryMansionCase AUTO_STORY_MANSION_CASES[] = {
    {
        .input = {.name = "13.01 Cinnabar to the Mansion's 3F statue", .body = []{ return test_row(ROW_13_01); }},
        .expectation = {.description = "through 1F and 2F to 3F's (12, 6), nobody met"},
    },
    {
        .input = {.name = "13.02 3F's opening, the hole, 1F's stairs", .body = []{ return test_row(ROW_13_02); }},
        .expectation = {.description = "through the declared opening and the hole to B1F's (34, 29)"},
    },
    {
        .input = {.name = "13.03 B1F's two statues, the Elixir and TM14", .body = []{ return test_row(ROW_13_03); }},
        .expectation = {.description = "through the declared opening to (27, 6), nobody but Lewis met"},
    },
    {
        .input = {.name = "13.04 the SECRET KEY and the Escape Rope", .body = []{ return test_row(ROW_13_04); }},
        .expectation = {.description = "through the declared opening to the key, out by the rope to (8, 4)"},
    },
};

} // namespace


void add_tests_AutoStoryMansion(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_MANSION_CASES){
        database.add<Test_AutoStoryMansion>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
