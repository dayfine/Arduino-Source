/*  Map Pack Loader Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_MapPackLoader.h"
#include "Pokemon_MapPackLoader_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

const std::string SAMPLE_TOWN_JSON = R"({
    "name": "SampleTown",
    "source": {"decomp_file": "sample.json", "revision": "1", "date": "2026-11-22"},
    "tiles": [
        "#####",
        "#...#",
        "#.#.#",
        "#.D.#",
        "#####"
    ],
    "points": {
        "door": [2, 3],
        "line.1": [1, 1],
        "line.2": [2, 1],
        "line.3": [3, 1]
    },
    "warps": [
        {"x": 2, "y": 3, "enter": "walk_into", "to_map": "Center", "to_point": "exit"}
    ],
    "objects": [
        {"name": "guide", "kind": "npc", "x": 1, "y": 1},
        {"name": "guard", "kind": "npc", "x": 3, "y": 2}
    ],
    "flag_sets": {
        "after_gym1": ["FLAG_A", "FLAG_B"]
    }
})";

const std::string SAMPLE_ROUTE_JSON = R"({
    "name": "SampleRoute",
    "source": {"decomp_file": "route.json", "revision": "1", "date": "2026-11-22"},
    "tiles": [
        "###",
        "#.#",
        "###"
    ],
    "flag_sets": {
        "after_gym1": ["FLAG_B", "FLAG_C"]
    }
})";

struct LoadMapCase{
    struct Input{
        std::string_view lookup_name = {};
        std::string json_key = {};
        std::string json_text = {};
    };
    struct Expectation{
        bool throws_parse = false;
        bool throws_internal = false;
        std::string map_name = {};
    };

    std::string_view name = {};
    Input input = {};
    Expectation expectation = {};
};

const LoadMapCase LOAD_MAP_CASES[] = {
    {
        .name = "parses matching embedded map",
        .input = {.lookup_name = "SampleTown", .json_key = "SampleTown", .json_text = SAMPLE_TOWN_JSON},
        .expectation = {.map_name = "SampleTown"},
    },
    {
        .name = "rejects mismatched map name in JSON",
        .input = {.lookup_name = "WrongName", .json_key = "WrongName", .json_text = SAMPLE_TOWN_JSON},
        .expectation = {.throws_parse = true},
    },
    {
        .name = "throws InternalProgramError when map is absent",
        .input = {.lookup_name = "MissingMap", .json_key = "SampleTown", .json_text = SAMPLE_TOWN_JSON},
        .expectation = {.throws_internal = true},
    },
};

class Test_LoadEmbeddedMap : public UnitTest{
public:
    explicit Test_LoadEmbeddedMap(const LoadMapCase& scenario)
        : UnitTest(std::format("Pokemon::MapPackLoader - load: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::pair<std::string, std::string>> pack = {
            {m_scenario.input.json_key, m_scenario.input.json_text},
        };
        try{
            const MapData map = load_embedded_map(pack, m_scenario.input.lookup_name, "Maps/");
            if (m_scenario.expectation.throws_parse || m_scenario.expectation.throws_internal){
                return "expected exception but load_embedded_map() succeeded";
            }
            if (map.name != m_scenario.expectation.map_name){
                return std::format("expected name {}, got {}", m_scenario.expectation.map_name, map.name);
            }
            return true;
        }catch (const ParseException&){
            return m_scenario.expectation.throws_parse ? UnitTestResult(true) : UnitTestResult("unexpected ParseException");
        }catch (const InternalProgramError&){
            return m_scenario.expectation.throws_internal ? UnitTestResult(true) : UnitTestResult("unexpected InternalProgramError");
        }
    }

private:
    LoadMapCase m_scenario;
};

struct QueryPackCase{
    struct Input{
        std::string_view point_name = {};
        std::string_view region_name = {};
        std::string_view flag_set = {};
    };
    struct Expectation{
        TilePos point = {};
        size_t region_size = 0;
        std::set<std::string> flags = {};
    };

    std::string_view name = {};
    Input input = {};
    Expectation expectation = {};
};

const QueryPackCase QUERY_PACK_CASES[] = {
    {
        .name = "loads pack, points, numbered region, and unioned flag set",
        .input = {.point_name = "door", .region_name = "line", .flag_set = "after_gym1"},
        .expectation = {.point = {2, 3}, .region_size = 3, .flags = {"FLAG_A", "FLAG_B", "FLAG_C"}},
    },
};

class Test_QueryMapPack : public UnitTest{
public:
    explicit Test_QueryMapPack(const QueryPackCase& scenario)
        : UnitTest(std::format("Pokemon::MapPackLoader - query: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::pair<std::string, std::string>> embedded = {
            {"SampleTown", SAMPLE_TOWN_JSON},
            {"SampleRoute", SAMPLE_ROUTE_JSON},
        };
        const auto by_name = load_embedded_map_pack(embedded);
        if (by_name.size() != 2){
            return "expected 2 maps in pack";
        }
        std::vector<MapData> maps;
        for (const auto& [name, data] : by_name){
            maps.push_back(data);
        }
        const TilePos pt = require_map_point(maps, "SampleTown", m_scenario.input.point_name);
        if (pt != m_scenario.expectation.point){
            return "require_map_point() returned wrong tile";
        }
        const std::vector<TilePos> region = require_numbered_map_region(
            by_name.at("SampleTown"), m_scenario.input.region_name
        );
        if (region.size() != m_scenario.expectation.region_size){
            return "require_numbered_map_region() returned wrong tile count";
        }
        if (collect_map_flag_set(maps, m_scenario.input.flag_set) != m_scenario.expectation.flags){
            return "collect_map_flag_set() returned wrong flags";
        }
        return true;
    }

private:
    QueryPackCase m_scenario;
};

struct CropMapCase{
    struct Input{
        TilePos corner = {};
        int width = 0;
        int height = 0;
    };
    struct Expectation{
        size_t objects = 0;
        size_t warps = 0;
        TilePos first_object = {};
        TilePos first_warp = {};
    };

    std::string_view name = {};
    Input input = {};
    Expectation expectation = {};
};

const CropMapCase CROP_MAP_CASES[] = {
    {
        .name = "shifts objects and warps inside crop and drops outside ones",
        .input = {.corner = {2, 2}, .width = 3, .height = 3},
        .expectation = {.objects = 1, .warps = 1, .first_object = {1, 0}, .first_warp = {0, 1}},
    },
};

class Test_CropMapData : public UnitTest{
public:
    explicit Test_CropMapData(const CropMapCase& scenario)
        : UnitTest(std::format("Pokemon::MapPackLoader - crop: {}", scenario.name))
        , m_scenario(scenario)
    {}

    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::vector<std::pair<std::string, std::string>> pack = {
            {"SampleTown", SAMPLE_TOWN_JSON},
        };
        const MapData full = load_embedded_map(pack, "SampleTown");
        const MapData sub = crop_map_data(
            full, m_scenario.input.corner, m_scenario.input.width, m_scenario.input.height
        );
        if (sub.objects.size() != m_scenario.expectation.objects
            || sub.warps.size() != m_scenario.expectation.warps
        ){
            return "cropped object or warp count mismatch";
        }
        if (TilePos{sub.objects[0].x, sub.objects[0].y} != m_scenario.expectation.first_object
            || TilePos{sub.warps[0].x, sub.warps[0].y} != m_scenario.expectation.first_warp
        ){
            return "cropped object or warp coordinate mismatch";
        }
        return true;
    }

private:
    CropMapCase m_scenario;
};

} // namespace


void add_tests_MapPackLoader(UnitTestDatabase& database){
    for (const LoadMapCase& scenario : LOAD_MAP_CASES){
        database.add<Test_LoadEmbeddedMap>(scenario);
    }
    for (const QueryPackCase& scenario : QUERY_PACK_CASES){
        database.add<Test_QueryMapPack>(scenario);
    }
    for (const CropMapCase& scenario : CROP_MAP_CASES){
        database.add<Test_CropMapData>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
