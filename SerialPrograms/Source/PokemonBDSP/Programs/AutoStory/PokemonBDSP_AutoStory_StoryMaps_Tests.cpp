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
#include "PokemonBDSP_AutoStory_StoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::StoryStep;
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
UnitTestResult test_route202_catch_grass(Logger& logger){
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
                    return "From " + name + " to the catch grass: the walk may end on " + describe(maps, pos) + ", not in the grass.";
                }
            }
            logger.log(std::format("From {} to the catch grass: {} legs, ending in the grass.", name, legs->size()));
        }
        return true;
}

//  The table's step `name` in row `row`.
const StoryStep& table_step(const std::string& row, const std::string& name){
    for (const StoryCheckpoint& checkpoint : story_checkpoints()){
        for (const StoryStep& step : checkpoint.steps){
            if (checkpoint.id == row && step.name == name){
                return step;
            }
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No step " + name + " in " + row);
}

//  00.04b's way to Sandgem's Pokemon Center from the road's last row,
//  wherever on it the run in stopped (lab-B 2026-11-09 20:09: two columns
//  short): west to the row's wall and south onto the Center's sign, the
//  table's walls, then its taps round the sign to below the door, each
//  open, the door straight north of where they end.
UnitTestResult test_sandgem_center_approach(Logger& logger){
        const StoryWorld town = alone("SandgemTown");
        const TilePos sign = map_point("SandgemTown", "pokemon_center_sign");
        const TilePos door = map_point("SandgemTown", "pokemon_center_door");
        const TilePos entry = map_point("SandgemTown", "route_202_entry_stop");
        if (town.tile(sign) != TileKind::SIGN || town.tile(door) != TileKind::DOOR){
            return std::format("The Center's sign {} and door {} are not a sign and a door.",
                Pokemon::to_string(sign), Pokemon::to_string(door));
        }
        const StoryStep& walls = table_step("00.04b", "above_center_sign");
        const StoryStep& taps = table_step("00.04b", "below_center_door");
        const TilePos above_sign = sign.next(Direction::NORTH);
        for (TilePos start = entry; standable(town, start); start = start.next(Direction::WEST)){
            TilePos pos = start;
            for (const Pokemon::StoryLeg& leg : walls.legs){
                pos = slide_to_wall(town, pos, leg.direction).stop;
            }
            if (!(pos == above_sign)){
                return std::format("From {} the walls end at {}, not above the sign {}.",
                    Pokemon::to_string(start), Pokemon::to_string(pos), Pokemon::to_string(sign));
            }
        }
        TilePos pos = above_sign;
        for (const Pokemon::StoryLeg& leg : taps.legs){
            for (int tap = 0; tap < leg.tiles; tap++){
                const std::optional<TilePos> next = step(town, pos, leg.direction);
                if (!next){
                    return std::format("{} from {} is blocked.", Pokemon::direction_name(leg.direction), Pokemon::to_string(pos));
                }
                pos = *next;
            }
        }
        if (!(pos == door.next(Direction::SOUTH))){
            return std::format("The taps end at {}, not below the door {}.", Pokemon::to_string(pos), Pokemon::to_string(door));
        }
        logger.log(std::format("Sandgem: from the road's last row by walls to {}, {} tap legs to below the door {}.",
            Pokemon::to_string(above_sign), taps.legs.size(), Pokemon::to_string(door)));
        return true;
}

//  After training, 00.04 walks from wherever the training left us into
//  Sandgem, to where the road stops above the Mart, with the default costs
//  (heal_at_sandgem_center()): from the whole area and from each of its
//  tiles alone, a plan must exist and end there.
UnitTestResult test_training_to_sandgem(Logger& logger){
        const StoryWorld& maps = story_maps();
        const TilePos entry = maps.to_world(MAP_SANDGEM_TOWN, map_point("SandgemTown", "route_202_entry_stop"));
        const std::vector<TilePos> area = story_after_training_area();
        std::vector<std::vector<TilePos>> starts{area};
        for (const TilePos& pos : area){
            starts.push_back({pos});
        }
        for (const std::vector<TilePos>& from : starts){
            const auto legs = plan_robust_route(maps, from, [&](const TilePos& pos){ return pos == entry; }, RobustPlanCosts());
            const std::string name = from.size() == 1 ? describe(maps, from[0]) : "the whole training area";
            if (!legs || legs->empty()){
                return "From " + name + " into Sandgem: no plan.";
            }
            if (legs->back().after.size() != 1 || !(legs->back().after[0] == entry)){
                return "From " + name + " into Sandgem: the plan does not end on " + describe(maps, entry) + ".";
            }
            logger.log(std::format("From {} into Sandgem: {} legs.", name, legs->size()));
        }
        return true;
}

//  The Oreburgh Gym's inside: the column up from the entrance runs to Roark
//  with nothing in the way but the two Youngsters' sight lines, which is
//  what 01.04's "north until a trainer stops us" relies on.
UnitTestResult test_oreburgh_gym_column(Logger& logger){
        const StoryWorld gym = alone("OreburghGym");
        const TilePos entrance = map_point("OreburghGym", "entrance");
        const TilePos roark = map_point("OreburghGym", "roark");
        if (!standable(gym, entrance) || gym.tile(entrance.next(Direction::SOUTH)) != TileKind::DOOR){
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

//  Oreburgh Mine B1F (the entrance hall):
//  The hallway connects Oreburgh City above with the Coal Room below.
//  Row 1 contains warp doors (11..13, 1) leading to the city.
//  Row 21 contains warp doors (11..13, 21) leading to the coal room.
//  Column 12 is unobstructed south from landing (12, 1) to warp door (12, 21).
//  Column 11 is unobstructed north from landing (11, 21) to warp door (11, 1).
UnitTestResult test_oreburgh_mine_hall(Logger& logger){
    const StoryWorld hall = alone("OreburghMineB1F");
    for (int x = 11; x <= 13; x++){
        if (hall.tile({x, 1}) != TileKind::DOOR){
            return std::format("OreburghMineB1F ({}, 1) is not a DOOR tile (exit to city).", x);
        }
        if (hall.tile({x, 21}) != TileKind::DOOR){
            return std::format("OreburghMineB1F ({}, 21) is not a DOOR tile (entrance to coal room).", x);
        }
    }
    for (int y = 2; y <= 20; y++){
        if (!standable(hall, {12, y})){
            return std::format("OreburghMineB1F column 12 row {} is blocked; expected clear path to coal room.", y);
        }
        if (!standable(hall, {11, y})){
            return std::format("OreburghMineB1F column 11 row {} is blocked; expected clear path to city exit.", y);
        }
    }
    logger.log("OreburghMineB1F: columns 11 and 12 are clear between row 1 and row 21 warp doors.");
    return true;
}

//  01.04: the D-pad taps west from below Oreburgh's Center door end below
//  the Gym's door, where the step north goes in. Red with 20 taps: they ended
//  on (27, 21), a tile east of the door (lab-B, 2026-11-08 14:51).
UnitTestResult test_oreburgh_center_to_gym_taps(Logger& logger){
        const RouteRecords::RouteRecord* record = nullptr;
        for (const RouteRecords::RouteRecord& item : RouteRecords::all()){
            if (item.checkpoint == "01.04" && item.map_file == "OreburghCity"){
                record = &item;
            }
        }
        if (record == nullptr || !record->start || record->legs.empty()){
            return "No 01.04 record on Oreburgh City with a start tile and legs.";
        }
        const StoryWorld city = alone("OreburghCity");
        const RouteRecords::RouteLeg& west = record->legs.front();
        const TilePos end = trace_legs(city, *record->start, {{west.direction, west.tiles, false}}).back();
        const TilePos doorstep = map_point("OreburghCity", "gym_doorstep");
        if (end != doorstep){
            return std::format("{} taps west end on ({}, {}), not below the Gym's door ({}, {}).", west.tiles, end.x, end.y, doorstep.x, doorstep.y);
        }
        logger.log(std::format("Oreburgh: {} taps west from below the Center's door reach the Gym's doorstep.", west.tiles));
        return true;
}

//  Each traceable record with a start tile starts on ground, and its legs
//  counted in whole tiles trace on its map up to the first leg that ends
//  some other way.
UnitTestResult test_route_records(Logger& logger){
        for (const RouteRecords::RouteRecord& record : RouteRecords::all()){
            const std::string name = record.checkpoint + " " + record.title;
            const StoryWorld world = alone(record.map_file);
            if (!record.traceable || !record.start){
                continue;
            }
            if (!standable(world, *record.start)){
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
                logger.log(std::format("{}: {} counted legs to {}.", name, counted.size(), describe(world, end)));
            }catch (InternalProgramError& e){
                return name + ": " + e.message();
            }
        }
        return true;
}


struct StoryMapsInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct StoryMapsExpectation{
};

struct StoryMapsCase{
    StoryMapsInput input;
    StoryMapsExpectation expectation;
};

class Test_StoryMaps : public UnitTest{
public:
    explicit Test_StoryMaps(StoryMapsCase test_case)
        : UnitTest("PokemonBDSP::AutoStory - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return m_case.input.body(logger);
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    StoryMapsCase m_case;
};

} // namespace


const StoryMapsCase STORY_MAPS_CASES[] = {
    {
        .input = {"Route 202: the catch patch and the walk to it", test_route202_catch_grass},
        .expectation = {},
    },
    {
        .input = {"Sandgem Town: from the road's last row by walls to below the Center's door", test_sandgem_center_approach},
        .expectation = {},
    },
    {
        .input = {"Route 202: from every tile of the training area into Sandgem", test_training_to_sandgem},
        .expectation = {},
    },
    {
        .input = {"Oreburgh Gym: the column up to Roark", test_oreburgh_gym_column},
        .expectation = {},
    },
    {
        .input = {"Oreburgh Mine B1F: the entrance hall corridor and warps", test_oreburgh_mine_hall},
        .expectation = {},
    },
    {
        .input = {"Oreburgh: 01.04's taps from the Center's door to the Gym's", test_oreburgh_center_to_gym_taps},
        .expectation = {},
    },
    {
        .input = {"Route records: counted legs trace on their maps", test_route_records},
        .expectation = {},
    },
};

void add_tests_AutoStory_StoryMaps(UnitTestDatabase& database){
    for (const auto& item : STORY_MAPS_CASES){
        database.add<Test_StoryMaps>(item);
    }
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
