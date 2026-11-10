/*  AutoStory Scripted Rooms Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_CodedRooms.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PokemonCenterRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"
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

//  On the sim, with the town's map and the Center's coded room: in from
//  below the door to the nurse, out again to below the door.
std::string in_and_out(const std::string& town, const std::string& center){
    std::vector<MapData> maps{load_map(town)};
    const std::optional<std::string> added = add_pokemon_center_room(maps, 0);
    if (added != center){
        return town + ": no door to " + center + ".";
    }
    const TilePos door = maps[0].points.at(center + "_door");
    const MapPosition below_door{town, door.next(Direction::SOUTH)};
    OverworldSim sim(maps, below_door, Direction::NORTH);
    StoryPose at{town, below_door.point, Direction::NORTH};
    std::string failure = row_step_on_sim(sim, at, StoryTable::center_enter("to_nurse", center, "test"), "test");
    if (!failure.empty()){
        return failure;
    }
    const Pokemon::Pose outside = StoryTable::at(town.c_str(), below_door.point.x, below_door.point.y, Direction::SOUTH);
    failure = row_step_on_sim(sim, at, StoryTable::center_exit("out", outside, "test"), "test");
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

} // namespace


void add_tests_AutoStoryRooms(UnitTestDatabase& database){
    for (const CenterCase& item : CENTER_CASES){
        database.add<Test_AutoStoryCenter>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
