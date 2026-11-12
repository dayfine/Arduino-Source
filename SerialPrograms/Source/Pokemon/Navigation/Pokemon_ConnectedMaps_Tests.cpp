/*  Connected Maps Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A town with a route north of it and a lake east of it, joined by their
 *  places on one world grid (connect_by_world()), and a house the town's
 *  door warps into:
 *
 *      world x:  -1  0  1  2  3  4  5
 *      y 0..2:   [ Route (6 wide) ]
 *      y 3..5:      [ Town (4) ][Lake]
 *
 */

#include <format>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_ConnectedMaps.h"
#include "Pokemon_ConnectedMaps_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{
namespace{


MapData open_map(std::string name, int width, int height){
    MapData map;
    map.name = std::move(name);
    map.tiles = Grid<TileKind>{width, height, std::vector<TileKind>((size_t)width * height, TileKind::OPEN)};
    return map;
}

MapData on_world(MapData map, const TilePos& offset){
    map.world = MapWorld{"Test", offset};
    return map;
}

const std::vector<MapData>& test_maps(){
    static const std::vector<MapData> maps = []{
        std::vector<MapData> ret{
            on_world(open_map("Town", 4, 3), {0, 3}),
            on_world(open_map("Route", 6, 3), {-1, 0}),
            on_world(open_map("Lake", 2, 3), {4, 3}),
            open_map("House", 2, 2),
        };
        ret[3].points["door"] = {0, 1};
        connect_by_world(ret);
        return ret;
    }();
    return maps;
}

//  A tile named by its map.
struct NamedPlace{
    std::string_view map;
    TilePos tile;
};

std::string describe(const ConnectedMaps& world, const std::optional<MapPlace>& place){
    if (!place){
        return "nowhere";
    }
    return std::format("{} {}", world.map(place->map).name, to_string(place->tile));
}

//  Where a step or a warp should put the player, or nothing for nowhere.
struct PlaceExpectation{
    std::optional<NamedPlace> place;
};

//  Why `actual` is not `expected`, if it is not.
std::optional<std::string> mismatch(const ConnectedMaps& world, const std::optional<MapPlace>& actual, const PlaceExpectation& expectation){
    const std::optional<NamedPlace>& expected = expectation.place;
    std::optional<MapPlace> wanted;
    if (expected){
        wanted = MapPlace{*world.find(expected->map), expected->tile};
    }
    if (actual == wanted){
        return std::nullopt;
    }
    return std::format("Got {}, expected {}.", describe(world, actual), describe(world, wanted));
}


struct StepInput{
    std::string_view name;
    NamedPlace from;
    Direction direction;
};
struct StepCase{
    StepInput input;
    PlaceExpectation expectation;
};

class Test_ConnectedMaps_Step : public UnitTest{
public:
    explicit Test_ConnectedMaps_Step(StepCase scenario)
        : UnitTest(std::format("Pokemon::ConnectedMaps - {}", scenario.input.name)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ConnectedMaps world(test_maps());
        const StepInput& input = m_scenario.input;
        const MapPlace from{*world.find(input.from.map), input.from.tile};
        if (std::optional<std::string> error = mismatch(world, world.neighbour(from, input.direction), m_scenario.expectation)){
            return *error;
        }
        return true;
    }
private:
    StepCase m_scenario;
};

const StepCase STEP_CASES[] = {
    {
        .input = {.name = "a step on the same map", .from = {"Town", {1, 1}}, .direction = Direction::EAST},
        .expectation = {.place = NamedPlace{"Town", {2, 1}}},
    },
    {
        .input = {.name = "north onto the route, shifted by its offset", .from = {"Town", {0, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = NamedPlace{"Route", {1, 2}}},
    },
    {
        .input = {.name = "south off the route into the town", .from = {"Route", {1, 2}}, .direction = Direction::SOUTH},
        .expectation = {.place = NamedPlace{"Town", {0, 0}}},
    },
    {
        .input = {.name = "east into the lake", .from = {"Town", {3, 2}}, .direction = Direction::EAST},
        .expectation = {.place = NamedPlace{"Lake", {0, 2}}},
    },
    {
        .input = {.name = "west into the town", .from = {"Lake", {0, 1}}, .direction = Direction::WEST},
        .expectation = {.place = NamedPlace{"Town", {3, 1}}},
    },
    {
        .input = {.name = "north out of the lake onto the route's last column", .from = {"Lake", {0, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = NamedPlace{"Route", {5, 2}}},
    },
    {
        .input = {.name = "north out of the lake past the route's span", .from = {"Lake", {1, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = std::nullopt},
    },
    {
        .input = {.name = "an edge no map joins", .from = {"Town", {0, 1}}, .direction = Direction::WEST},
        .expectation = {.place = std::nullopt},
    },
    {
        .input = {.name = "past the joined map's span", .from = {"Route", {0, 2}}, .direction = Direction::SOUTH},
        .expectation = {.place = std::nullopt},
    },
    {
        .input = {.name = "a map off the world grid joins nothing", .from = {"House", {1, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = std::nullopt},
    },
};


struct WarpInput{
    std::string_view name;
    MapWarp warp;
};
struct WarpCase{
    WarpInput input;
    PlaceExpectation expectation;
};

class Test_ConnectedMaps_Warp : public UnitTest{
public:
    explicit Test_ConnectedMaps_Warp(WarpCase scenario)
        : UnitTest(std::format("Pokemon::ConnectedMaps - {}", scenario.input.name)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ConnectedMaps world(test_maps());
        if (std::optional<std::string> error = mismatch(world, world.warp_arrival(m_scenario.input.warp), m_scenario.expectation)){
            return *error;
        }
        return true;
    }
private:
    WarpCase m_scenario;
};

const WarpCase WARP_CASES[] = {
    {
        .input = {.name = "a warp to a loaded map's point", .warp = {.to = WarpDestination{"House", "door"}}},
        .expectation = {.place = NamedPlace{"House", {0, 1}}},
    },
    {
        .input = {.name = "a warp to a map that isn't loaded", .warp = {.to = WarpDestination{"Cave", "door"}}},
        .expectation = {.place = std::nullopt},
    },
    {
        .input = {.name = "a warp to a point the map lacks", .warp = {.to = WarpDestination{"House", "stairs"}}},
        .expectation = {.place = std::nullopt},
    },
    {
        .input = {.name = "a warp that leads nowhere", .warp = {}},
        .expectation = {.place = std::nullopt},
    },
};


struct StalePlaceInput{
    std::string_view name;
    size_t map;
};
struct StalePlaceCase{
    StalePlaceInput input;
};

//  A place whose map index is past the loaded maps (from another world, or
//  one that has since shrunk) is an InternalProgramError, not a read past
//  the end.
class Test_ConnectedMaps_StalePlace : public UnitTest{
public:
    explicit Test_ConnectedMaps_StalePlace(StalePlaceCase scenario)
        : UnitTest(std::format("Pokemon::ConnectedMaps - {}", scenario.input.name)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ConnectedMaps world(test_maps());
        const MapPlace stale{m_scenario.input.map, {0, 0}};
        try{
            const std::optional<MapPlace> place = world.neighbour(stale, Direction::NORTH);
            return std::format("Map {} of {} was read: got {}, expected an InternalProgramError.",
                stale.map, world.size(), place ? to_string(place->tile) : "nowhere");
        }catch (const InternalProgramError&){
            return true;
        }
    }
private:
    StalePlaceCase m_scenario;
};

const StalePlaceCase STALE_PLACE_CASES[] = {
    {.input = {.name = "a map index one past the last is rejected", .map = 4}},
    {.input = {.name = "a map index far past the last is rejected", .map = 1000}},
};


} // namespace

void add_tests_ConnectedMaps(UnitTestDatabase& database){
    for (const StepCase& scenario : STEP_CASES){
        database.add<Test_ConnectedMaps_Step>(scenario);
    }
    for (const WarpCase& scenario : WARP_CASES){
        database.add<Test_ConnectedMaps_Warp>(scenario);
    }
    for (const StalePlaceCase& scenario : STALE_PLACE_CASES){
        database.add<Test_ConnectedMaps_StalePlace>(scenario);
    }
}


} // namespace Pokemon
} // namespace PokemonAutomation
