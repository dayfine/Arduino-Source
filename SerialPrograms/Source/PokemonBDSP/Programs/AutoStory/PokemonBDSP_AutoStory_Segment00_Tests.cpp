/*  BDSP AutoStory Segment 00 Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <optional>
#include <utility>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/Navigation/Pokemon_MapSet.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"
#include "Data/PokemonBDSP_AutoStory_OreburghGym.h"
#include "Data/PokemonBDSP_AutoStory_Route202.h"
#include "Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "Data/PokemonBDSP_AutoStory_Segment00Lines.h"
#include "Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Segment00_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

using Pokemon::Direction;
using Pokemon::MapSet;
using Pokemon::MapTile;
using Pokemon::TileGrid;
using Pokemon::TilePos;
using Pokemon::TileType;
using Segment00Lines::SceneTrigger;
using Segment00Lines::WalkedLine;

std::string describe(const MapTile& tile){
    return tile.map + " " + Pokemon::to_string(tile.pos);
}

const SceneTrigger* find_trigger(const std::string& name){
    for (const SceneTrigger& trigger : Segment00Lines::scene_triggers()){
        if (trigger.name == name){
            return &trigger;
        }
    }
    return nullptr;
}

//  Plans one stretch of a line with the waypoint costs and returns the leg,
//  or an error. The stretch must be the one straight leg a human took: to a
//  wall or open, as the grid has it, ending exactly on the corner.
struct Stretch{
    Pokemon::TileLeg leg;
    std::string error;
};
Stretch plan_stretch(const MapSet& maps, const TilePos& from, const TilePos& to, const std::string& name){
    const auto plan = Pokemon::plan_robust_route(maps.world(), std::vector<TilePos>{from},
        [&](const TilePos& pos){ return pos == to; }, waypoint_plan_costs());
    if (!plan){
        return {{}, name + ": no plan."};
    }
    if (plan->size() != 1){
        std::string text = name + ": " + std::to_string(plan->size()) + " legs, expected the one straight leg:";
        for (const Pokemon::PlannedLeg& leg : *plan){
            text += " " + to_string(leg.leg);
        }
        return {{}, text};
    }
    const Pokemon::PlannedLeg& leg = (*plan)[0];
    if (leg.after.size() != 1 || !(leg.after[0] == to)){
        return {{}, name + ": the leg " + to_string(leg.leg) + " does not end exactly on the corner."};
    }
    return {leg.leg, ""};
}

//  Every tile a line stands on, its start first, as local tiles; logs each
//  stretch's leg. Empty with `error` set when a stretch does not plan.
std::vector<MapTile> line_tiles(Logger& logger, const MapSet& maps, const WalkedLine& line, std::string& error){
    const std::string name = line.checkpoint + " " + line.name;
    std::vector<MapTile> tiles{line.corners[0]};
    TilePos at = maps.to_world(line.corners[0].map, line.corners[0].pos);
    std::string text = name + ": from " + describe(line.corners[0]);
    for (size_t i = 1; i < line.corners.size(); i++){
        const MapTile& corner = line.corners[i];
        const TilePos goal = maps.to_world(corner.map, corner.pos);
        const Stretch stretch = plan_stretch(maps, at, goal, name + " stretch " + std::to_string(i) + " to " + describe(corner));
        if (!stretch.error.empty()){
            error = stretch.error;
            return {};
        }
        text += "\n    " + to_string(stretch.leg) + " to " + describe(corner);
        for (size_t k = 0; k < stretch.leg.tiles; k++){
            at = at.next(stretch.leg.direction);
            const std::optional<MapTile> local = maps.to_local(at);
            if (!local){
                error = name + ": the line leaves the maps at " + Pokemon::to_string(at) + ".";
                return {};
            }
            tiles.push_back(*local);
        }
    }
    logger.log(text);
    return tiles;
}

//  The last tile steps into the line's scene; no earlier tile is in any
//  scene armed while the line is walked (an inert one is only logged).
std::string check_triggers(Logger& logger, const WalkedLine& line, const std::vector<MapTile>& tiles){
    const std::string name = line.checkpoint + " " + line.name;
    for (const std::string& live : line.live_triggers){
        if (find_trigger(live) == nullptr){
            return name + ": no scene trigger named \"" + live + "\".";
        }
    }
    const auto armed = [&](const std::string& trigger){
        return std::ranges::find(line.live_triggers, trigger) != line.live_triggers.end();
    };
    if (!line.ends_in.empty() && !armed(line.ends_in)){
        return name + ": the scene it ends in, \"" + line.ends_in + "\", is not among its live triggers.";
    }
    for (size_t i = 0; i < tiles.size(); i++){
        for (const SceneTrigger& trigger : Segment00Lines::scene_triggers()){
            if (!Segment00Lines::trigger_covers(trigger, tiles[i])){
                continue;
            }
            const bool live = armed(trigger.name);
            const bool last = i + 1 == tiles.size();
            logger.log(name + ": tile " + std::to_string(i + 1) + " of " + std::to_string(tiles.size()) + ", " + describe(tiles[i]) +
                ", is in \"" + trigger.name + "\" (" + (live ? "armed" : "inert: " + trigger.when) + ").");
            if (live && !(last && trigger.name == line.ends_in)){
                return name + ": steps into the armed scene \"" + trigger.name + "\" at " + describe(tiles[i]) + ", before its end.";
            }
        }
    }
    if (!line.ends_in.empty() && !Segment00Lines::trigger_covers(*find_trigger(line.ends_in), tiles.back())){
        return name + ": its last tile " + describe(tiles.back()) + " is not in \"" + line.ends_in + "\".";
    }
    return "";
}

//  The tiles the route records start and end on, each of which must lie on
//  one of the lines.
struct NamedTile{
    const char* name;
    MapTile tile;
};
std::vector<NamedTile> named_tiles(){
    return {
        {"HOUSE_DOORSTEP", {Segment00Lines::MAP_TWINLEAF_TOWN, Segment00Lines::HOUSE_DOORSTEP}},
        {"BARRY_DOORSTEP", {Segment00Lines::MAP_TWINLEAF_TOWN, Segment00Lines::BARRY_DOORSTEP}},
        {"TWINLEAF_NORTH_EXIT", {Segment00Lines::MAP_TWINLEAF_TOWN, Segment00Lines::TWINLEAF_NORTH_EXIT}},
        {"ROUTE_201_ARRIVAL", {MAP_ROUTE_201, Segment00Lines::ROUTE_201_ARRIVAL}},
        {"BARRY_WARNING", {MAP_ROUTE_201, Segment00Lines::BARRY_WARNING}},
        {"LAKEFRONT_ARRIVAL", {Segment00Lines::MAP_VERITY_LAKEFRONT, Segment00Lines::LAKEFRONT_ARRIVAL}},
        {"BELOW_LAKE_ENTRANCE", {Segment00Lines::MAP_VERITY_LAKEFRONT, Segment00Lines::BELOW_LAKE_ENTRANCE}},
        {"LAKE_ENTRANCE", {Segment00Lines::MAP_VERITY_LAKEFRONT, Segment00Lines::LAKE_ENTRANCE}},
        {"ROUTE_201_RETURN", {MAP_ROUTE_201, Segment00Lines::ROUTE_201_RETURN}},
        {"ROWAN_STOP", {MAP_ROUTE_201, Segment00Lines::ROWAN_STOP}},
        {"ROUTE_201_EAST_END", {MAP_ROUTE_201, Segment00Lines::ROUTE_201_EAST_END}},
        {"SANDGEM_ARRIVAL", {MAP_SANDGEM_TOWN, Segment00Lines::SANDGEM_ARRIVAL}},
        {"DAWN_STOP", {MAP_SANDGEM_TOWN, Segment00Lines::DAWN_STOP}},
    };
}

//  The lines 00.01 to 00.03 would walk (Data/PokemonBDSP_AutoStory_Segment00Lines):
//  every stretch plans as one straight leg on the decomp grids, and each
//  line enters its scene's trigger rectangle on its last tile only. The
//  named tiles the route records start from all lie on the lines.
class Test_Segment00Lines : public UnitTest{
public:
    Test_Segment00Lines()
        : UnitTest("PokemonBDSP::AutoStory - Segment 00: the lines into each scene")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const MapSet& maps = Segment00Lines::segment00_maps();
        for (const SceneTrigger& trigger : Segment00Lines::scene_triggers()){
            if (trigger.width <= 0 || trigger.height <= 0){
                return "Scene trigger \"" + trigger.name + "\" has no tiles.";
            }
            maps.to_world(trigger.map, trigger.corner);     //  throws for a map not in the set
        }
        std::vector<MapTile> walked;
        for (const WalkedLine& line : Segment00Lines::lines()){
            if (line.corners.size() < 2){
                return line.checkpoint + " " + line.name + ": a line needs two corners.";
            }
            std::string error;
            const std::vector<MapTile> tiles = line_tiles(logger, maps, line, error);
            if (!error.empty()){
                return error;
            }
            error = check_triggers(logger, line, tiles);
            if (!error.empty()){
                return error;
            }
            walked.insert(walked.end(), tiles.begin(), tiles.end());
        }
        for (const NamedTile& tile : named_tiles()){
            const bool on_a_line = std::ranges::any_of(walked, [&](const MapTile& t){
                return t.map == tile.tile.map && t.pos == tile.tile.pos;
            });
            if (!on_a_line){
                return std::string(tile.name) + " " + describe(tile.tile) + " is on none of the lines.";
            }
        }
        return true;
    }
};

//  The Oreburgh Gym's inside (Data/PokemonBDSP_AutoStory_OreburghGym): the
//  column up from the entrance runs to Roark with nothing in the way but the
//  two Youngsters' sight lines, which is what 01.04's "north until a trainer
//  stops us" relies on.
class Test_OreburghGymGrid : public UnitTest{
public:
    Test_OreburghGymGrid()
        : UnitTest("PokemonBDSP::AutoStory - Oreburgh Gym: the column up to Roark")
    {}

    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        const TileGrid& gym = OreburghGym::grid();
        if (!gym.standable(OreburghGym::ENTRANCE)){
            return "The entrance tile is not standable.";
        }
        if (gym.at(OreburghGym::ENTRANCE.next(Direction::SOUTH)) != TileType::DOOR){
            return "The door is not below the entrance tile.";
        }
        if (gym.at(OreburghGym::ROARK) != TileType::NPC){
            return "Roark is not on an NPC tile.";
        }
        const Pokemon::WallHit up = Pokemon::walk_to_wall(gym, OreburghGym::ENTRANCE, Direction::NORTH);
        if (!(up.wall == OreburghGym::ROARK)){
            return "North from the entrance stops at " + Pokemon::to_string(up.stop) + ", not in front of Roark.";
        }
        logger.log("Oreburgh Gym: north from the entrance walks " + std::to_string(up.tiles) + " tiles to Roark.");
        for (const Pokemon::TrainerSight& trainer : OreburghGym::trainer_sights()){
            if (gym.at(trainer.pos) != TileType::NPC){
                return trainer.name + " is not on an NPC tile.";
            }
            const std::vector<TilePos> seen = Pokemon::sight_tiles(gym, trainer);
            const bool sees_column = std::ranges::any_of(seen, [&](const TilePos& pos){
                return pos.x == OreburghGym::ENTRANCE.x && pos.y < OreburghGym::ENTRANCE.y && pos.y > OreburghGym::ROARK.y;
            });
            if (!sees_column){
                return trainer.name + " does not see the column up to Roark.";
            }
            std::string text = "Oreburgh Gym: " + trainer.name + " sees";
            for (const TilePos& pos : seen){
                text += " " + Pokemon::to_string(pos);
            }
            logger.log(text);
        }
        return true;
    }
};

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

//  indoors() decides when leave_door_south() is out: r257 walked on south
//  past Sandgem's road into a house after leaving the Center, because the
//  evening tint made the outside as warm as a room. Synthetic screens: the
//  evening outside (warm all over), a room (warm, black along the bottom),
//  a fade (black all over), and warm with a thin dark edge (a shadow).
struct IndoorsCase{
    std::string name;
    uint32_t fill;
    double dark_bottom;     //  how much of the screen's height, from the bottom, is black
    bool indoors;
};

class Test_IndoorsAnyLight : public UnitTest{
public:
    Test_IndoorsAnyLight()
        : UnitTest("PokemonBDSP::AutoStory - indoors() under the evening tint")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<IndoorsCase> cases{
            {.name = "evening outside, orange all over", .fill = 0xffe0a040, .dark_bottom = 0, .indoors = false},
            {.name = "a room: orange, black below its floor", .fill = 0xffe0a040, .dark_bottom = 0.10, .indoors = true},
            {.name = "a fade: black all over", .fill = 0xff000000, .dark_bottom = 0, .indoors = false},
            {.name = "orange with a thin shadow at the bottom", .fill = 0xffe0a040, .dark_bottom = 0.01, .indoors = false},
            {.name = "daytime grass", .fill = 0xff60b040, .dark_bottom = 0.10, .indoors = false},
        };
        for (const IndoorsCase& item : cases){
            ImageRGB32 screen(1920, 1080);
            const size_t dark_from = (size_t)(1080 * (1 - item.dark_bottom));
            for (size_t y = 0; y < 1080; y++){
                for (size_t x = 0; x < 1920; x++){
                    screen.pixel(x, y) = y >= dark_from ? 0xff000000 : item.fill;
                }
            }
            if (indoors(screen) != item.indoors){
                return item.name + (item.indoors ? ": read as outside." : ": read as inside.");
            }
        }
        return true;
    }
};

}


void add_tests_AutoStory_Segment00(UnitTestDatabase& database){
    database.add<Test_Segment00Lines>();
    database.add<Test_OreburghGymGrid>();
    database.add<Test_Route202CatchGrass>();
    database.add<Test_SandgemCenterApproach>();
    database.add<Test_IndoorsAnyLight>();
}


}
}
}
