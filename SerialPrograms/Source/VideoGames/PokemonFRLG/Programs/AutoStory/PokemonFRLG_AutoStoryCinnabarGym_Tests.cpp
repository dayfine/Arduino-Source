/*  AutoStory Cinnabar Gym Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows 14.01 and 14.02 (StoryData/frlg-14.01.plan.json, 14.02) on the
 *  overworld sim: from outside the Pokemon Mansion to Cinnabar's nurse,
 *  into the gym and through its six quiz doors to below Blaine; and from
 *  Blaine back out through the doors, past Bill, to the nurse. And the
 *  lead's attacks against Blaine's party.
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
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string GYM = "CinnabarIsland_Gym";

struct AutoStoryCinnabarGymInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryCinnabarGymExpectation{
    std::string_view description;
};

struct AutoStoryCinnabarGymCase{
    AutoStoryCinnabarGymInput input;
    AutoStoryCinnabarGymExpectation expectation;
};

class Test_AutoStoryCinnabarGym : public UnitTest{
public:
    explicit Test_AutoStoryCinnabarGym(AutoStoryCinnabarGymCase test_case)
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
    AutoStoryCinnabarGymCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    for (const StoryCheckpoint& row : story_checkpoints()){
        if (row.id == id){
            return row;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
}

//  The rows on the sim, which can't open the quiz doors (the scripts swap
//  their metatiles) or walk into a Center with no map. Kept across sims:
//  the trainers fought.
struct GymWalk{
    StoryPose at;
    std::optional<OverworldSim> sim;
    std::vector<std::string> fought;

    explicit GymWalk(StoryPose start)
        : at(std::move(start))
    {
        place();
    }
    //  A fresh sim on `at`, after a step it can't walk.
    void place(){
        if (sim){
            const std::vector<std::string>& beaten = sim->state().trainers_beaten;
            fought.insert(fought.end(), beaten.begin(), beaten.end());
        }
        sim.emplace(story_maps(), MapPosition{at.map, at.tile}, *at.facing);
    }
    std::vector<std::string> all_fought() const{
        std::vector<std::string> all = fought;
        const std::vector<std::string>& beaten = sim->state().trainers_beaten;
        all.insert(all.end(), beaten.begin(), beaten.end());
        return all;
    }
};

//  A step of a row on the sim: row_step_on_sim(), but a step through a
//  quiz door, the way into the Center and Blaine's battle, which a talk
//  starts, leave the player where the table says.
std::string gym_step_on_sim(const StoryStep& step, const std::string& id, GymWalk& walk){
    const bool door = step.kind == StoryStepKind::STEP && (step.name.starts_with("into_door_") || step.name.starts_with("through_door_"));
    const bool center = step.action == CENTER_ENTER_VARIANT;
    if (door || center || (step.kind == StoryStepKind::BATTLE && step.name == "blaine")){
        walk.at = *story_pose(Pokemon::pose_of(*step.pose_after));
        if (door || center){
            walk.place();
        }
        return "";
    }
    return row_step_on_sim(*walk.sim, walk.at, step, id);
}

//  Row `id`'s steps on the sim from `walk.at`: the failure, or empty.
std::string row_on_sim(std::string_view id, GymWalk& walk){
    const StoryCheckpoint& row = checkpoint(id);
    for (const StoryStep& step : row.steps){
        if (const std::string failure = gym_step_on_sim(step, row.id, walk); !failure.empty()){
            return std::format("{}: ended on {}; {}", id, to_string(walk.sim->state().position), failure);
        }
        if (!walk.sim->state().violations.empty()){
            return std::format("{}/{}: a violation on the way: {}", id, step.name, walk.sim->state().violations.back());
        }
    }
    return "";
}

//  Whether `tile` of the gym is a wall on the map.
bool gym_wall(const TilePos& tile){
    for (const MapData& map : story_maps()){
        if (map.name == GYM){
            return map.tiles.at(tile) == TileKind::WALL;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No map {}.", GYM));
}

Direction behind(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return Direction::SOUTH;
    case Direction::SOUTH:
        return Direction::NORTH;
    case Direction::EAST:
        return Direction::WEST;
    default:
        return Direction::EAST;
    }
}

//  Each quiz door is shut on the map, one barrier tile, with floor on both
//  sides: the tiles 14.01 steps over.
UnitTestResult test_quiz_doors_on_map(){
    const StoryCheckpoint& row = checkpoint("14.01");
    int doors = 0;
    for (const StoryStep& step : row.steps){
        if (!step.name.starts_with("into_door_")){
            continue;
        }
        const StoryPose door = *story_pose(Pokemon::pose_of(*step.pose_after));
        const TilePos before = door.tile.next(behind(*door.facing));
        const TilePos after = door.tile.next(*door.facing);
        if (!gym_wall(door.tile) || gym_wall(before) || gym_wall(after)){
            return std::format("{}: expected a barrier on {} between floor tiles.", step.name, to_string(MapPosition{GYM, door.tile}));
        }
        doors++;
    }
    return doors == 6 ? UnitTestResult(true) : UnitTestResult(std::format("Expected 6 quiz doors, found {}.", doors));
}

//  14.01 from outside the Mansion: the nurse, the gym and its six quiz
//  doors, nobody fought, to below Blaine, where 14.02 starts; then 14.02
//  from there out of the gym to the nurse.
UnitTestResult test_gym_rows(){
    GymWalk walk(*story_pose(Pokemon::pose_of(*checkpoint("14.01").start)));
    if (const std::string failure = row_on_sim("14.01", walk); !failure.empty()){
        return failure;
    }
    const MapPosition below_blaine{GYM, {5, 5}};
    if (!(walk.sim->state().position == below_blaine) || walk.sim->state().facing != Direction::NORTH){
        return std::format("14.01: ended on {}, not below Blaine {} facing north.", to_string(walk.sim->state().position), to_string(below_blaine));
    }
    if (!walk.all_fought().empty()){
        return "14.01: fought a gym trainer; the quiz is answered instead.";
    }
    const StoryPose next = *story_pose(Pokemon::pose_of(*checkpoint("14.02").start));
    if (next.map != GYM || !(next.tile == below_blaine.point)){
        return "14.02 doesn't start where 14.01 ends.";
    }
    if (const std::string failure = row_on_sim("14.02", walk); !failure.empty()){
        return failure;
    }
    if (!walk.all_fought().empty()){
        return "14.02: fought a gym trainer on the way out.";
    }
    if (walk.at.map != "CinnabarIsland_PokemonCenter_1F"){
        return std::format("14.02: ended on {}, not at Cinnabar's nurse.", walk.at.map);
    }
    return true;
}

//  The lead as pokeguy brings it to Blaine: Surf (12.01, over Water Gun),
//  Water Pulse, Bite, Mega Kick. It picks Surf against all four of
//  Blaine's Fire types.
UnitTestResult test_lead_moves_against_blaine(){
    PartyMember lead;
    lead.species = "blastoise";
    lead.moves_read = true;
    lead.level = 40;
    lead.moves = {{"surf", 15}, {"water-pulse", 20}, {"bite", 25}, {"mega-kick", 5}};
    for (std::string_view foe : {"growlithe", "ponyta", "rapidash", "arcanine"}){
        if (attack_slot_against(lead, {std::string(foe)}) != 0){
            return std::format("Against {}: expected Surf.", foe);
        }
    }
    return true;
}

const AutoStoryCinnabarGymCase AUTO_STORY_CINNABAR_GYM_CASES[] = {
    {
        .input = {.name = "Cinnabar Gym's quiz doors are shut on the map", .body = test_quiz_doors_on_map},
        .expectation = {.description = "each of 14.01's door steps crosses one barrier tile between floor"},
    },
    {
        .input = {.name = "14.01 through the quiz to below Blaine and 14.02 back to the nurse", .body = test_gym_rows},
        .expectation = {.description = "no gym trainer fought; ends at Cinnabar's nurse"},
    },
    {
        .input = {.name = "the lead's attacks against Blaine", .body = test_lead_moves_against_blaine},
        .expectation = {.description = "Surf against each Fire type"},
    },
};

} // namespace


void add_tests_AutoStoryCinnabarGym(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_CINNABAR_GYM_CASES){
        database.add<Test_AutoStoryCinnabarGym>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
