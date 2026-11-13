/*  AutoStory Viridian Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Viridian Gym's spinner tiles (rows 17.02 to 17.04b,
 *  StoryData/frlg-17.0N.plan.json) on the overworld sim.
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
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSpin.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryRooms.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryRows17_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryViridianInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct AutoStoryViridianExpectation{
    std::string_view description;
};

struct AutoStoryViridianCase{
    AutoStoryViridianInput input;
    AutoStoryViridianExpectation expectation;
};

class Test_AutoStoryViridian : public UnitTest{
public:
    explicit Test_AutoStoryViridian(AutoStoryViridianCase test_case)
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
    AutoStoryViridianCase m_case;
};

//  Viridian Gym's arrow tiles (the map file's glyphs): stepped onto, the
//  player slides until a stop tile or a wall, turned by each arrow met.
UnitTestResult check_gym_slide(const TilePos& from, const TilePos& end, Direction facing, int tiles){
    const MapData* gym = nullptr;
    for (const MapData& map : story_maps()){
        if (map.name == "ViridianCity_Gym"){
            gym = &map;
        }
    }
    if (gym == nullptr){
        return "No ViridianCity_Gym map.";
    }
    const std::optional<SpinSlide> slide = spin_slide(*gym, PlayerState{from, ELEVATION_GROUND, false});
    if (!slide){
        return std::format("No slide from {}.", to_string(from));
    }
    if (!(slide->end.position == end) || slide->facing != facing || slide->tiles != tiles){
        return std::format(
            "From {}: slid {} tile(s) to {} facing {}; expected {} to {} facing {}.",
            to_string(from), slide->tiles, to_string(slide->end.position), direction_name(slide->facing),
            tiles, to_string(end), direction_name(facing)
        );
    }
    return true;
}

UnitTestResult test_gym_east_arrow(){
    return check_gym_slide({11, 3}, {17, 3}, Direction::EAST, 6);
}
UnitTestResult test_gym_south_arrow_chain(){
    return check_gym_slide({18, 3}, {18, 14}, Direction::SOUTH, 11);
}
UnitTestResult test_gym_north_arrow(){
    return check_gym_slide({0, 20}, {0, 11}, Direction::NORTH, 9);
}

const AutoStoryViridianCase AUTO_STORY_VIRIDIAN_CASES[] = {
    {
        .input = {.name = "Viridian Gym: the east arrow at (11, 3) slides to its stop", .body = test_gym_east_arrow},
        .expectation = {.description = "6 tiles east to (17, 3)"},
    },
    {
        .input = {.name = "Viridian Gym: the south arrows at (18, 3) slide to the stop below", .body = test_gym_south_arrow_chain},
        .expectation = {.description = "11 tiles south to (18, 14)"},
    },
    {
        .input = {.name = "Viridian Gym: the north arrow at (0, 20) slides to its stop", .body = test_gym_north_arrow},
        .expectation = {.description = "9 tiles north to (0, 11)"},
    },
};

} // namespace


void add_tests_AutoStoryViridian(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_VIRIDIAN_CASES){
        database.add<Test_AutoStoryViridian>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
