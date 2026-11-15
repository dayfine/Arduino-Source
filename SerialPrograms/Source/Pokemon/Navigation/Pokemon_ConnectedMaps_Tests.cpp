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

//  Why `actual` is not `expected`, if it is not.
std::optional<std::string> mismatch(const ConnectedMaps& world, const std::optional<MapPlace>& actual, const std::optional<NamedPlace>& expected){
    std::optional<MapPlace> wanted;
    if (expected){
        wanted = MapPlace{*world.find(expected->map), expected->tile};
    }
    if (actual == wanted){
        return std::nullopt;
    }
    return std::format("Got {}, expected {}.", describe(world, actual), describe(world, wanted));
}


struct StepCase{
    struct Input{
        NamedPlace from;
        Direction direction;
    };
    //  Where the step should put the player, or nothing for nowhere.
    struct Expectation{
        std::optional<NamedPlace> place;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

class Test_ConnectedMaps_Step : public UnitTest{
public:
    explicit Test_ConnectedMaps_Step(StepCase scenario)
        : UnitTest(std::format("Pokemon::ConnectedMaps - {}", scenario.name)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ConnectedMaps world(test_maps());
        const StepCase::Input& input = m_scenario.input;
        const MapPlace from{*world.find(input.from.map), input.from.tile};
        if (std::optional<std::string> error = mismatch(world, world.neighbour(from, input.direction), m_scenario.expectation.place)){
            return *error;
        }
        return true;
    }
private:
    StepCase m_scenario;
};

const StepCase STEP_CASES[] = {
    {
        .name = "a step on the same map",
        .input = {.from = {"Town", {1, 1}}, .direction = Direction::EAST},
        .expectation = {.place = NamedPlace{"Town", {2, 1}}},
    },
    {
        .name = "north onto the route, shifted by its offset",
        .input = {.from = {"Town", {0, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = NamedPlace{"Route", {1, 2}}},
    },
    {
        .name = "south off the route into the town",
        .input = {.from = {"Route", {1, 2}}, .direction = Direction::SOUTH},
        .expectation = {.place = NamedPlace{"Town", {0, 0}}},
    },
    {
        .name = "east into the lake",
        .input = {.from = {"Town", {3, 2}}, .direction = Direction::EAST},
        .expectation = {.place = NamedPlace{"Lake", {0, 2}}},
    },
    {
        .name = "west into the town",
        .input = {.from = {"Lake", {0, 1}}, .direction = Direction::WEST},
        .expectation = {.place = NamedPlace{"Town", {3, 1}}},
    },
    {
        .name = "north out of the lake onto the route's last column",
        .input = {.from = {"Lake", {0, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = NamedPlace{"Route", {5, 2}}},
    },
    {
        .name = "north out of the lake past the route's span",
        .input = {.from = {"Lake", {1, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = std::nullopt},
    },
    {
        .name = "an edge no map joins",
        .input = {.from = {"Town", {0, 1}}, .direction = Direction::WEST},
        .expectation = {.place = std::nullopt},
    },
    {
        .name = "past the joined map's span",
        .input = {.from = {"Route", {0, 2}}, .direction = Direction::SOUTH},
        .expectation = {.place = std::nullopt},
    },
    {
        .name = "a map off the world grid joins nothing",
        .input = {.from = {"House", {1, 0}}, .direction = Direction::NORTH},
        .expectation = {.place = std::nullopt},
    },
};


struct WarpCase{
    struct Input{
        MapWarp warp;
    };
    //  Where the warp should put the player, or nothing for nowhere.
    struct Expectation{
        std::optional<NamedPlace> place;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

class Test_ConnectedMaps_Warp : public UnitTest{
public:
    explicit Test_ConnectedMaps_Warp(WarpCase scenario)
        : UnitTest(std::format("Pokemon::ConnectedMaps - {}", scenario.name)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ConnectedMaps world(test_maps());
        if (std::optional<std::string> error = mismatch(world, world.warp_arrival(m_scenario.input.warp), m_scenario.expectation.place)){
            return *error;
        }
        return true;
    }
private:
    WarpCase m_scenario;
};

const WarpCase WARP_CASES[] = {
    {
        .name = "a warp to a loaded map's point",
        .input = {.warp = {.to = WarpDestination{"House", "door"}}},
        .expectation = {.place = NamedPlace{"House", {0, 1}}},
    },
    {
        .name = "a warp to a map that isn't loaded",
        .input = {.warp = {.to = WarpDestination{"Cave", "door"}}},
        .expectation = {.place = std::nullopt},
    },
    {
        .name = "a warp to a point the map lacks",
        .input = {.warp = {.to = WarpDestination{"House", "stairs"}}},
        .expectation = {.place = std::nullopt},
    },
    {
        .name = "a warp that leads nowhere",
        .input = {.warp = {}},
        .expectation = {.place = std::nullopt},
    },
};


struct StalePlaceCase{
    struct Input{
        size_t map;
    };
    struct Expectation{
        //  Whether reading the place throws InternalProgramError.
        bool rejected = false;
    };
    std::string_view name;
    Input input;
    Expectation expectation;
};

//  A place whose map index is past the loaded maps (from another world, or
//  one that has since shrunk) is an InternalProgramError, not a read past
//  the end.
class Test_ConnectedMaps_StalePlace : public UnitTest{
public:
    explicit Test_ConnectedMaps_StalePlace(StalePlaceCase scenario)
        : UnitTest(std::format("Pokemon::ConnectedMaps - {}", scenario.name)), m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ConnectedMaps world(test_maps());
        const MapPlace stale{m_scenario.input.map, {0, 0}};
        bool rejected = false;
        try{
            world.neighbour(stale, Direction::NORTH);
        }catch (const InternalProgramError&){
            rejected = true;
        }
        if (rejected != m_scenario.expectation.rejected){
            return std::format("Map {} of {} was {}, expected it {}.", stale.map, world.size(),
                rejected ? "rejected" : "read", m_scenario.expectation.rejected ? "rejected" : "read");
        }
        return true;
    }
private:
    StalePlaceCase m_scenario;
};

const StalePlaceCase STALE_PLACE_CASES[] = {
    {.name = "the last map index is read", .input = {.map = 3}, .expectation = {.rejected = false}},
    {.name = "a map index one past the last is rejected", .input = {.map = 4}, .expectation = {.rejected = true}},
    {.name = "a map index far past the last is rejected", .input = {.map = 1000}, .expectation = {.rejected = true}},
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
