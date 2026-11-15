/*  AutoStory Scripted Rooms Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_CodedRooms.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PokemonCenterRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryFieldSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct CenterInput{
    //  The town or route whose Center it is.
    std::string town;
};
struct CenterExpectation{
    std::string center;
};
struct CenterCase{
    CenterInput input;
    CenterExpectation expectation;
};

//  The Center's coded room has the tiles of its map in the lock.
std::string same_tiles(const std::string& center){
    const MapData mapped = load_map(center);
    const MapData coded = pokemon_center_room(center, {});
    if (mapped.tiles.width != coded.tiles.width || mapped.tiles.height != coded.tiles.height){
        return center + ": the coded room is not the map's size.";
    }
    for (int y = 0; y < coded.tiles.height; y++){
        for (int x = 0; x < coded.tiles.width; x++){
            if (tile_at(mapped, TilePos{x, y}) != tile_at(coded, TilePos{x, y})){
                return center + ": the coded room differs at (" + std::to_string(x) + ", " + std::to_string(y) + ").";
            }
        }
    }
    return "";
}

//  The town's map with the Center's coded room, and the tile below its
//  door; nothing if the town has no door to `center`.
struct CenterTown{
    std::vector<MapData> maps;
    MapPosition below_door;
};
std::optional<CenterTown> center_town(const std::string& town, const std::string& center){
    std::vector<MapData> maps{load_map(town)};
    if (add_pokemon_center_room(maps, 0) != center){
        return std::nullopt;
    }
    const TilePos door = maps[0].points.at(center + "_door");
    return CenterTown{std::move(maps), MapPosition{town, door.next(Direction::SOUTH)}};
}

StoryStep heal_step(){
    StoryStep step;
    step.kind = Pokemon::StoryStepKind::HEAL;
    step.name = "heal";
    return step;
}

//  On the sim, with the town's map and the Center's coded room: in from
//  below the door to the nurse, the nurse, out again to below the door.
std::string in_and_out(const std::string& town, const std::string& center){
    const std::optional<CenterTown> place = center_town(town, center);
    if (!place){
        return town + ": no door to " + center + ".";
    }
    const MapPosition& below_door = place->below_door;
    OverworldSim sim(place->maps, below_door, Direction::NORTH);
    StoryPose at{town, below_door.point, Direction::NORTH};
    std::string failure = row_step_on_sim(sim, at, center_enter_step("to_nurse", center), "test");
    if (!failure.empty()){
        return failure;
    }
    failure = row_step_on_sim(sim, at, heal_step(), "test");
    if (!failure.empty()){
        return failure;
    }
    const Pokemon::GridPose outside{town, {below_door.point}, Direction::SOUTH};
    failure = row_step_on_sim(sim, at, center_exit_step("out", outside), "test");
    if (!failure.empty()){
        return failure;
    }
    if (!sim.state().violations.empty()){
        return town + ": " + sim.state().violations.front();
    }
    return "";
}

class Test_AutoStoryCenter : public UnitTest{
public:
    explicit Test_AutoStoryCenter(CenterCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryRooms - Center: " + test_case.expectation.center)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string& center = m_case.expectation.center;
        if (std::string failure = same_tiles(center); !failure.empty()){
            return failure;
        }
        if (std::string failure = in_and_out(m_case.input.town, center); !failure.empty()){
            return failure;
        }
        return true;
    }

private:
    CenterCase m_case;
};

//  The seven Centers in the map lock.
const CenterCase CENTER_CASES[] = {
    {.input = {.town = "ViridianCity"}, .expectation = {.center = "ViridianCity_PokemonCenter_1F"}},
    {.input = {.town = "PewterCity"}, .expectation = {.center = "PewterCity_PokemonCenter_1F"}},
    {.input = {.town = "Route4"}, .expectation = {.center = "Route4_PokemonCenter_1F"}},
    {.input = {.town = "CeruleanCity"}, .expectation = {.center = "CeruleanCity_PokemonCenter_1F"}},
    {.input = {.town = "VermilionCity"}, .expectation = {.center = "VermilionCity_PokemonCenter_1F"}},
    {.input = {.town = "Route10"}, .expectation = {.center = "Route10_PokemonCenter_1F"}},
    {.input = {.town = "LavenderTown"}, .expectation = {.center = "LavenderTown_PokemonCenter_1F"}},
};

//  A Center visit's step on the sim that must fail, and how.
struct CenterFailureInput{
    //  Where the player starts: inside the Center (at the nurse) or below
    //  its door.
    bool at_nurse = false;
    //  The step: "heal", or "out" (center_exit).
    std::string_view step;
    //  Each warp shows its black and leaves the player where they were.
    bool doors_stay_shut = false;
};
struct CenterFailureExpectation{
    //  Part of the step's failure.
    std::string_view failure;
};
struct CenterFailureCase{
    std::string name;
    CenterFailureInput input;
    CenterFailureExpectation expectation;
};

std::string center_failure(const CenterFailureInput& input){
    const std::string town = "ViridianCity";
    const std::string center = "ViridianCity_PokemonCenter_1F";
    const std::optional<CenterTown> place = center_town(town, center);
    if (!place){
        return "no door to " + center + ".";
    }
    SimScript script;
    if (input.doors_stay_shut){
        script.black_without_warp = {1};
    }
    const MapPosition start = input.at_nurse ? MapPosition{center, CENTER_NURSE_FRONT} : place->below_door;
    OverworldSim sim(place->maps, start, Direction::NORTH, script);
    StoryPose at{start.map, start.point, Direction::NORTH};
    const Pokemon::GridPose outside{town, {place->below_door.point}, Direction::SOUTH};
    const StoryStep step = input.step == "heal" ? heal_step() : center_exit_step("out", outside);
    return row_step_on_sim(sim, at, step, "test");
}

class Test_AutoStoryCenterFailure : public UnitTest{
public:
    explicit Test_AutoStoryCenterFailure(CenterFailureCase test_case)
        : UnitTest("PokemonFRLG::AutoStoryRooms - Center fails: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string failure = center_failure(m_case.input);
        if (failure.find(m_case.expectation.failure) == std::string::npos){
            return "expected a failure with \"" + std::string(m_case.expectation.failure) + "\", got \"" + failure + "\".";
        }
        return true;
    }

private:
    CenterFailureCase m_case;
};

const CenterFailureCase CENTER_FAILURE_CASES[] = {
    {
        //  12.03 (console B, 2026-11-13 04:16): the player was on Route
        //  21's water, not in the Center, and the heal's presses were
        //  counted as the nurse's.
        .name = "a heal with no Center floor fails before the nurse",
        .input = {.at_nurse = false, .step = "heal", .doors_stay_shut = false},
        .expectation = {.failure = "not in a Pokemon Center"},
    },
    {
        //  BDSP r143a's exit: a fade seen, the player still inside.
        .name = "an exit that stays inside fails",
        .input = {.at_nurse = true, .step = "out", .doors_stay_shut = true},
        .expectation = {.failure = "still inside the Pokemon Center"},
    },
};

} // namespace


void add_tests_AutoStoryRooms(UnitTestDatabase& database){
    for (const CenterCase& item : CENTER_CASES){
        database.add<Test_AutoStoryCenter>(item);
    }
    for (const CenterFailureCase& item : CENTER_FAILURE_CASES){
        database.add<Test_AutoStoryCenterFailure>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
