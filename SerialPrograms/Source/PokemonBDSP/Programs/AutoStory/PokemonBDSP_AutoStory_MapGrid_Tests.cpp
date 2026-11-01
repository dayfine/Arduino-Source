/*  BDSP AutoStory Map Grid Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Pokemon/Navigation/Pokemon_MapSet.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_Route202.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_MapGrid_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::MapSet;
using Pokemon::TileGrid;
using Pokemon::TilePos;
using Pokemon::TileType;

//  00.04's training walks up and down the catch patch, three grass rows
//  between tree rows, and needs the walk there to end in its grass: r253
//  and r254 loaded the catch's save in the pocket behind the trees at
//  (21, 29), planned from the catch spot instead, and walked up and down
//  the lawn east of the grass for three minutes without a battle.
class Test_Route202CatchGrass : public UnitTest{
public:
    Test_Route202CatchGrass()
        : UnitTest("PokemonBDSP::AutoStory - Route 202: the catch patch and the walk to it")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const TileGrid& route = Route202::grid();
        for (int x = 15; x <= 20; x++){
            const Pokemon::WallHit up = Pokemon::walk_to_wall(route, {x, 27}, Direction::NORTH);
            const Pokemon::WallHit down = Pokemon::walk_to_wall(route, {x, 25}, Direction::SOUTH);
            if (!(up.stop == TilePos{x, 25}) || !(down.stop == TilePos{x, 27})){
                return "Catch patch column " + std::to_string(x) + ": north from row 27 stops at " +
                    Pokemon::to_string(up.stop) + ", south from row 25 at " + Pokemon::to_string(down.stop) +
                    "; expected rows 25 and 27, between tree rows.";
            }
        }
        const MapSet& maps = story_maps();
        const std::vector<TilePos> area = story_after_training_area();
        const auto in_grass = [&](const TilePos& pos){
            return maps.world().at(pos) == TileType::TALL_GRASS && std::find(area.begin(), area.end(), pos) != area.end();
        };
        //  As walk_route() plans: the route costs, else the default costs.
        //  From the whole area the catch's save may stand in, and from the
        //  pocket behind the trees alone, where r252 saved.
        const TilePos pocket = maps.to_world(MAP_ROUTE_202, {21, 29});
        const std::vector<std::pair<std::string, std::vector<TilePos>>> starts{
            {"the catch's save area", story_catch_save_area()},
            {"the pocket behind the trees", {pocket}},
        };
        std::optional<std::vector<Pokemon::PlannedLeg>> legs;
        for (const auto& [name, from] : starts){
            legs = Pokemon::plan_robust_route(maps.world(), from, in_grass, route_plan_costs());
            if (!legs){
                legs = Pokemon::plan_robust_route(maps.world(), from, in_grass, Pokemon::RobustPlanCosts());
            }
            if (!legs || legs->empty()){
                return "From " + name + " to the catch grass: no plan.";
            }
            for (const TilePos& pos : legs->back().after){
                if (!in_grass(pos)){
                    return "From " + name + " to the catch grass: the walk may end on " + maps.describe(pos) + ", not in the grass.";
                }
            }
            logger.log("From " + name + " to the catch grass: " + std::to_string(legs->size()) + " legs, ending in the grass.");
        }
        return true;
    }
};

//  00.04's way to Sandgem's Pokemon Center ends below the Center's sign,
//  walking north into it, then taps west along the doors' row to below the
//  door: the row must be open there, the door straight north of where the
//  taps end, and nothing else (a sign, the Mart's door) on the way.
class Test_SandgemCenterApproach : public UnitTest{
public:
    Test_SandgemCenterApproach()
        : UnitTest("PokemonBDSP::AutoStory - Sandgem Town: below the Center's sign to below its door")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const TileGrid& town = SandgemTown::grid();
        const TilePos sign = SandgemTown::POKEMON_CENTER_SIGN;
        const TilePos door = SandgemTown::POKEMON_CENTER_DOOR;
        if (town.at(sign) != TileType::SIGN || town.at(door) != TileType::DOOR || door.y != sign.y || door.x >= sign.x){
            return "The Center's sign " + Pokemon::to_string(sign) + " and door " + Pokemon::to_string(door) +
                " are not a sign and a door west of it on one row.";
        }
        TilePos pos = sign.next(Direction::SOUTH);
        for (int tap = 0; tap < sign.x - door.x; tap++){
            const std::optional<TilePos> next = town.step(pos, Direction::WEST);
            if (!next){
                return "West from " + Pokemon::to_string(pos) + " is blocked.";
            }
            if (town.at(next->next(Direction::NORTH)) != TileType::BLOCKED && *next != door.next(Direction::SOUTH)){
                return "Above " + Pokemon::to_string(*next) + " is not the Center's wall.";
            }
            pos = *next;
        }
        if (!(pos == door.next(Direction::SOUTH))){
            return "The taps end at " + Pokemon::to_string(pos) + ", not below the door " + Pokemon::to_string(door) + ".";
        }
        logger.log("Sandgem: below the sign " + Pokemon::to_string(sign.next(Direction::SOUTH)) + ", " +
            std::to_string(sign.x - door.x) + " taps west to below the door " + Pokemon::to_string(door) + ".");
        return true;
    }
};

//  After training, 00.04 walks from wherever the training left us into
//  Sandgem, to where the road stops above the Mart, with the default costs
//  (heal_at_sandgem_center()): from the whole area and from each of its
//  tiles alone, a plan must exist and end there.
class Test_TrainingToSandgem : public UnitTest{
public:
    Test_TrainingToSandgem()
        : UnitTest("PokemonBDSP::AutoStory - Route 202: from every tile of the training area into Sandgem")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const MapSet& maps = story_maps();
        const TilePos entry = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::ROUTE_202_ENTRY_STOP);
        const std::vector<TilePos> area = story_after_training_area();
        std::vector<std::vector<TilePos>> starts{area};
        for (const TilePos& pos : area){
            starts.push_back({pos});
        }
        for (const std::vector<TilePos>& from : starts){
            const auto legs = Pokemon::plan_robust_route(maps.world(), from,
                [&](const TilePos& pos){ return pos == entry; }, Pokemon::RobustPlanCosts());
            const std::string name = from.size() == 1 ? maps.describe(from[0]) : "the whole training area";
            if (!legs || legs->empty()){
                return "From " + name + " into Sandgem: no plan.";
            }
            if (legs->back().after.size() != 1 || !(legs->back().after[0] == entry)){
                return "From " + name + " into Sandgem: the plan does not end on " + maps.describe(entry) + ".";
            }
            logger.log("From " + name + " into Sandgem: " + std::to_string(legs->size()) + " legs.");
        }
        return true;
    }
};

}


void add_tests_AutoStory_MapGrids(UnitTestDatabase& database){
    database.add<Test_Route202CatchGrass>();
    database.add<Test_SandgemCenterApproach>();
    database.add<Test_TrainingToSandgem>();
}


}
}
}
