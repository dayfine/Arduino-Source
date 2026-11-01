/*  BDSP AutoStory Story Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's walks on its maps: Route 202's catch grass and the way into
 *  Sandgem, Sandgem's Center, the Oreburgh Gym's column, and the route
 *  records traced on their maps.
 *
 */

#include <algorithm>
#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Data/PokemonBDSP_AutoStory_Maps.h"
#include "Data/PokemonBDSP_AutoStory_PlanCosts.h"
#include "Data/PokemonBDSP_AutoStory_RouteRecords.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "PokemonBDSP_AutoStory_StoryMaps_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::TileKind;
using Pokemon::TilePos;

StoryWorld alone(const std::string& json_map){
    StoryWorld world;
    world.add(json_map, story_map(json_map), {0, 0});
    return world;
}

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
        const StoryWorld route = alone("Route202");
        for (int x = 15; x <= 20; x++){
            const WallHit up = slide_to_wall(route, {x, 27}, Direction::NORTH);
            const WallHit down = slide_to_wall(route, {x, 25}, Direction::SOUTH);
            if (!(up.stop == TilePos{x, 25}) || !(down.stop == TilePos{x, 27})){
                return std::format("Catch patch column {}: north from row 27 stops at {}, south from row 25 at {}; expected rows 25 and 27, between tree rows.",
                    x, Pokemon::to_string(up.stop), Pokemon::to_string(down.stop));
            }
        }
        const StoryWorld& maps = story_maps();
        const std::vector<TilePos> area = story_after_training_area();
        const auto in_grass = [&](const TilePos& pos){
            return maps.tile(pos) == TileKind::TALL_GRASS && std::find(area.begin(), area.end(), pos) != area.end();
        };
        //  As walk_route() plans: the route costs, else the default costs.
        //  From the whole area the catch's save may stand in, and from the
        //  pocket behind the trees alone, where r252 saved.
        const std::vector<std::pair<std::string, std::vector<TilePos>>> starts{
            {"the catch's save area", story_catch_save_area()},
            {"the pocket behind the trees", {maps.to_world(MAP_ROUTE_202, {21, 29})}},
        };
        for (const auto& [name, from] : starts){
            std::optional<std::vector<PlannedLeg>> legs = plan_robust_route(maps, from, in_grass, route_plan_costs());
            if (!legs){
                legs = plan_robust_route(maps, from, in_grass, RobustPlanCosts());
            }
            if (!legs || legs->empty()){
                return "From " + name + " to the catch grass: no plan.";
            }
            for (const TilePos& pos : legs->back().after){
                if (!in_grass(pos)){
                    return "From " + name + " to the catch grass: the walk may end on " + maps.describe(pos) + ", not in the grass.";
                }
            }
            logger.log(std::format("From {} to the catch grass: {} legs, ending in the grass.", name, legs->size()));
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
        const StoryWorld town = alone("SandgemTown");
        const TilePos sign = map_point("SandgemTown", "pokemon_center_sign");
        const TilePos door = map_point("SandgemTown", "pokemon_center_door");
        if (town.tile(sign) != TileKind::SIGN || town.tile(door) != TileKind::DOOR || door.y != sign.y || door.x >= sign.x){
            return std::format("The Center's sign {} and door {} are not a sign and a door west of it on one row.",
                Pokemon::to_string(sign), Pokemon::to_string(door));
        }
        TilePos pos = sign.next(Direction::SOUTH);
        for (int tap = 0; tap < sign.x - door.x; tap++){
            const std::optional<TilePos> next = town.step(pos, Direction::WEST);
            if (!next){
                return "West from " + Pokemon::to_string(pos) + " is blocked.";
            }
            if (town.tile(next->next(Direction::NORTH)) != TileKind::WALL && *next != door.next(Direction::SOUTH)){
                return "Above " + Pokemon::to_string(*next) + " is not the Center's wall.";
            }
            pos = *next;
        }
        if (!(pos == door.next(Direction::SOUTH))){
            return std::format("The taps end at {}, not below the door {}.", Pokemon::to_string(pos), Pokemon::to_string(door));
        }
        logger.log(std::format("Sandgem: below the sign {}, {} taps west to below the door {}.",
            Pokemon::to_string(sign.next(Direction::SOUTH)), sign.x - door.x, Pokemon::to_string(door)));
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
        const StoryWorld& maps = story_maps();
        const TilePos entry = maps.to_world(MAP_SANDGEM_TOWN, map_point("SandgemTown", "route_202_entry_stop"));
        const std::vector<TilePos> area = story_after_training_area();
        std::vector<std::vector<TilePos>> starts{area};
        for (const TilePos& pos : area){
            starts.push_back({pos});
        }
        for (const std::vector<TilePos>& from : starts){
            const auto legs = plan_robust_route(maps, from, [&](const TilePos& pos){ return pos == entry; }, RobustPlanCosts());
            const std::string name = from.size() == 1 ? maps.describe(from[0]) : "the whole training area";
            if (!legs || legs->empty()){
                return "From " + name + " into Sandgem: no plan.";
            }
            if (legs->back().after.size() != 1 || !(legs->back().after[0] == entry)){
                return "From " + name + " into Sandgem: the plan does not end on " + maps.describe(entry) + ".";
            }
            logger.log(std::format("From {} into Sandgem: {} legs.", name, legs->size()));
        }
        return true;
    }
};

//  The Oreburgh Gym's inside: the column up from the entrance runs to Roark
//  with nothing in the way but the two Youngsters' sight lines, which is
//  what 01.04's "north until a trainer stops us" relies on.
class Test_OreburghGymColumn : public UnitTest{
public:
    Test_OreburghGymColumn()
        : UnitTest("PokemonBDSP::AutoStory - Oreburgh Gym: the column up to Roark")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const StoryWorld gym = alone("OreburghGym");
        const TilePos entrance = map_point("OreburghGym", "entrance");
        const TilePos roark = map_point("OreburghGym", "roark");
        if (!gym.standable(entrance) || gym.tile(entrance.next(Direction::SOUTH)) != TileKind::DOOR){
            return std::string("The entrance tile is not standable above the door.");
        }
        if (!gym.occupied(roark)){
            return std::string("Nobody stands on Roark's tile.");
        }
        const WallHit up = slide_to_wall(gym, entrance, Direction::NORTH);
        if (!(up.wall == roark)){
            return "North from the entrance stops at " + Pokemon::to_string(up.stop) + ", not in front of Roark.";
        }
        logger.log(std::format("Oreburgh Gym: north from the entrance walks {} tiles to Roark.", up.tiles));
        for (const TrainerSight& trainer : oreburgh_gym_trainer_sights()){
            if (!gym.occupied(trainer.pos)){
                return trainer.name + " does not stand on their tile.";
            }
            const std::vector<TilePos> seen = sight_tiles(gym, trainer);
            const bool sees_column = std::ranges::any_of(seen, [&](const TilePos& pos){
                return pos.x == entrance.x && pos.y < entrance.y && pos.y > roark.y;
            });
            if (!sees_column){
                return trainer.name + " does not see the column up to Roark.";
            }
        }
        return true;
    }
};

//  Each traceable record with a start tile starts on ground, and its legs
//  counted in whole tiles trace on its map up to the first leg that ends
//  some other way.
class Test_RouteRecords : public UnitTest{
public:
    Test_RouteRecords()
        : UnitTest("PokemonBDSP::AutoStory - Route records: counted legs trace on their maps")
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        for (const RouteRecords::RouteRecord& record : RouteRecords::all()){
            const std::string name = record.checkpoint + " " + record.title;
            const StoryWorld world = alone(record.map_file);
            if (!record.traceable || !record.start){
                continue;
            }
            if (!world.standable(*record.start)){
                return name + ": starts on " + Pokemon::to_string(*record.start) + ", where nobody can stand.";
            }
            std::vector<TileLeg> counted;
            for (const RouteRecords::RouteLeg& leg : record.legs){
                if (leg.end != RouteRecords::LegEnd::COUNT || !leg.exact){
                    break;
                }
                counted.push_back({leg.direction, leg.tiles, false});
            }
            try{
                const TilePos end = trace_legs(world, *record.start, counted).back();
                logger.log(std::format("{}: {} counted legs to {}.", name, counted.size(), world.describe(end)));
            }catch (InternalProgramError& e){
                return name + ": " + e.message();
            }
        }
        return true;
    }
};

}


void add_tests_AutoStory_StoryMaps(UnitTestDatabase& database){
    database.add<Test_Route202CatchGrass>();
    database.add<Test_SandgemCenterApproach>();
    database.add<Test_TrainingToSandgem>();
    database.add<Test_OreburghGymColumn>();
    database.add<Test_RouteRecords>();
}


}
}
}
