/*  Versioned Runtime Map Tests
 *
 *  From: https://github.com/PokemonAutomation/
 */
#include <format>
#include <string>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Pokemon_MapData.h"
#include "Pokemon_RuntimeMap.h"
#ifdef PA_RUNTIME_MAP_STANDALONE
#include <iostream>
#else
#include "Pokemon_RuntimeMap_Tests.h"
#endif

namespace PokemonAutomation{
namespace Pokemon{
namespace{

struct RuntimeMapCase{
    struct Input{
        std::string_view fields;
    };

    struct Expectation{
        std::string_view error = {};
        std::string_view normalized_fields = {};
    };

    std::string_view name;
    Input input;
    Expectation expectation;
};

const std::string GOLDEN_V2 = R"(
    "format":"pokesim-runtime-map/2",
    "world":{"name":"Sinnoh","x":0,"y":0},
    "extension":{"zero":0,"false":false},
    "npcs":[
        {"name":"npc","coord":[0,0],"facing":"east","trainer":{"sight":1},"wander":{"min":[0,0],"max":[1,1]}},
        {"name":"npc","coord":[1,0],"hide_flag":"hidden"}
    ],
    "items":[{"name":"pickup","coord":[0,1],"item":"potion","quantity":1}],
    "signs":[{"name":"sign","coord":[1,1]}],
    "interactables":[{"name":"ball","coord":[1,0],"interaction":{"kind":"starter_choice","species":"squirtle"}}],
    "warps":[{"coord":[1,1],"enter":"walk_north","to_map":"House","to_point":"exit"}],
    "patches":[{"coord":[0,1],"glyph":"#","measured_by":"synthetic"}],
    "coord_events":[{"coord":[0,0],"width":1,"length":2,"var":0}]
)";

const std::string GOLDEN_V1 = R"(
    "world":{"name":"Sinnoh","x":0,"y":0},
    "extension":{"zero":0,"false":false},
    "objects":[
        {"name":"npc","kind":"npc","x":0,"y":0,"facing":"east","trainer":{"sight":1},"wander":{"x0":0,"y0":0,"x1":1,"y1":1}},
        {"name":"npc","kind":"npc","x":1,"y":0,"hide_flag":"hidden"},
        {"name":"pickup","kind":"item","x":0,"y":1,"item":"potion","quantity":1},
        {"name":"sign","kind":"sign","x":1,"y":1},
        {"name":"ball","kind":"item","x":1,"y":0,"interaction":{"kind":"starter_choice","species":"squirtle"}}
    ],
    "warps":[{"x":1,"y":1,"enter":"walk_north","to_map":"House","to_point":"exit"}],
    "patches":[{"x":0,"y":1,"glyph":"#","measured_by":"synthetic"}],
    "coord_events":[{"x":0,"y":0,"width":1,"length":2,"var":0}]
)";

const RuntimeMapCase RUNTIME_MAP_CASES[] = {
    {.name = "pickup derived name and default quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"source_event":3,"item":"potion"}])"}, .expectation = {.normalized_fields = R"("objects":[{"x":0,"y":1,"kind":"item","name":"item_3","source_event":3,"item":"potion","quantity":1}])"}},
    {.name = "pickup explicit name and multiple quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"name":"gift_cache","coord":[0,1],"source_event":3,"item":"potion","quantity":4}])"}, .expectation = {.normalized_fields = R"("objects":[{"x":0,"y":1,"kind":"item","name":"gift_cache","source_event":3,"item":"potion","quantity":4}])"}},
    {.name = "pickup named quantity default", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"name":"pickup","coord":[0,1],"item":"potion"}])"}, .expectation = {.normalized_fields = R"("objects":[{"x":0,"y":1,"kind":"item","name":"pickup","item":"potion","quantity":1}])"}},
    {.name = "pickup missing event", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"item":"potion"}])"}, .expectation = {.error = "source_event"}},
    {.name = "pickup negative event", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"item":"potion","source_event":-1}])"}, .expectation = {.error = "source_event"}},
    {.name = "pickup boolean event", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"item":"potion","source_event":true}])"}, .expectation = {.error = "source_event"}},
    {.name = "pickup overflow event", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"item":"potion","source_event":2147483648}])"}, .expectation = {.error = "source_event"}},
    {.name = "pickup zero quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"name":"p","item":"potion","quantity":0}])"}, .expectation = {.error = "quantity"}},
    {.name = "pickup negative quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"name":"p","item":"potion","quantity":-1}])"}, .expectation = {.error = "quantity"}},
    {.name = "pickup boolean quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"name":"p","item":"potion","quantity":true}])"}, .expectation = {.error = "quantity"}},
    {.name = "pickup null quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"name":"p","item":"potion","quantity":null}])"}, .expectation = {.error = "quantity"}},
    {.name = "pickup float quantity", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"name":"p","item":"potion","quantity":1.5}])"}, .expectation = {.error = "quantity"}},
    {.name = "pickup empty item", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,1],"name":"p","item":""}])"}, .expectation = {.error = "item"}},

    {.name = "complete normalized-value parity", .input = {.fields = GOLDEN_V2}, .expectation = {.normalized_fields = GOLDEN_V1}},
    {.name = "implicit v1", .input = {.fields = R"("objects":[{"name":"npc","kind":"npc","x":0,"y":0}])"}, .expectation = {}},
    {.name = "explicit v1", .input = {.fields = R"("format":"pokesim-runtime-map/1")"}, .expectation = {}},
    {.name = "v2 empty map", .input = {.fields = R"("format":"pokesim-runtime-map/2")"}, .expectation = {}},
    {.name = "v2 coordinates and inclusive wander", .input = {.fields = R"("format":"pokesim-runtime-map/2","npcs":[{"name":"npc","coord":[0,0],"wander":{"min":[0,0],"max":[1,1]}}])"}, .expectation = {}},
    {.name = "unsupported version", .input = {.fields = R"("format":"pokesim-runtime-map/99")"}, .expectation = {.error = "format"}},
    {.name = "nonstring version", .input = {.fields = R"("format":2)"}, .expectation = {.error = "format"}},
    {.name = "v1 typed section", .input = {.fields = R"("npcs":[])"}, .expectation = {.error = "npcs"}},
    {.name = "v2 legacy section", .input = {.fields = R"("format":"pokesim-runtime-map/2","objects":[])"}, .expectation = {.error = "objects"}},
    {.name = "mixed v2 position", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[0,0],"x":0}])"}, .expectation = {.error = "warps[0].coord"}},
    {.name = "mixed v1 position", .input = {.fields = R"("warps":[{"x":0,"y":0,"coord":[0,0]}])"}, .expectation = {.error = "warps[0].coord"}},
    {.name = "missing v2 position", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{}])"}, .expectation = {.error = "warps[0].coord"}},
    {.name = "short position", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[0]}])"}, .expectation = {.error = "warps[0].coord"}},
    {.name = "long position", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[0,0,0]}])"}, .expectation = {.error = "warps[0].coord"}},
    {.name = "floating coordinate", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[0.5,0]}])"}, .expectation = {.error = "coord[0]"}},
    {.name = "boolean coordinate", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[false,0]}])"}, .expectation = {.error = "coord[0]"}},
    {.name = "overflow coordinate", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[2147483648,0]}])"}, .expectation = {.error = "coord[0]"}},
    {.name = "outside map", .input = {.fields = R"("format":"pokesim-runtime-map/2","warps":[{"coord":[3,0]}])"}, .expectation = {.error = "outside"}},
    {.name = "mixed wander", .input = {.fields = R"("format":"pokesim-runtime-map/2","npcs":[{"name":"npc","coord":[0,0],"wander":{"min":[0,0],"max":[1,1],"x0":0}}])"}, .expectation = {.error = "wander.min"}},
    {.name = "reversed wander", .input = {.fields = R"("format":"pokesim-runtime-map/2","npcs":[{"name":"npc","coord":[0,0],"wander":{"min":[1,1],"max":[0,0]}}])"}, .expectation = {.error = "min <= max"}},
    {.name = "redundant kind", .input = {.fields = R"("format":"pokesim-runtime-map/2","signs":[{"name":"sign","coord":[0,0],"kind":"sign"}])"}, .expectation = {.error = "kind"}},
    {.name = "starter interaction", .input = {.fields = R"("format":"pokesim-runtime-map/2","interactables":[{"name":"ball","coord":[0,0],"interaction":{"kind":"starter_choice","species":"squirtle"}}])"}, .expectation = {}},
    {.name = "interaction has item", .input = {.fields = R"("format":"pokesim-runtime-map/2","interactables":[{"coord":[0,0],"interaction":{"kind":"starter_choice"},"item":"potion"}])"}, .expectation = {.error = "item reward"}},
    {.name = "interaction missing kind", .input = {.fields = R"("format":"pokesim-runtime-map/2","interactables":[{"coord":[0,0],"interaction":{}}])"}, .expectation = {.error = "interaction"}},
    {.name = "item has interaction", .input = {.fields = R"("format":"pokesim-runtime-map/2","items":[{"coord":[0,0],"interaction":{"kind":"starter_choice"}}])"}, .expectation = {.error = "interactables"}},
    {.name = "bad coordinate event", .input = {.fields = R"("format":"pokesim-runtime-map/2","coord_events":[{"coord":[0,true]}])"}, .expectation = {.error = "coord_events[0].coord[1]"}},
};

std::string map_document(std::string_view fields){
    const std::string_view head = R"({"name":"ContractMap","source":{"decomp_file":"synthetic","revision":"fixture","date":"2026-11-06"},"tiles":["..",".."],)";
    return std::format("{}{}}}", head, fields);
}

// This is also used by the standalone pin-based 75-map parity check.
std::string check_pair(const JsonValue& input, const JsonValue& expected){
    const JsonValue normalized = normalize_runtime_map(input, "Maps/parity.json");
    if (!(normalized == expected)){
        return "Full normalized JSON differs (including extension metadata/order).";
    }
    const MapData map = parse_map(input, "Maps/parity.json");
    const MapData legacy = parse_map(expected, "Maps/parity-v1.json");
    if (map.objects.size() != legacy.objects.size() || map.warps.size() != legacy.warps.size()){
        return "Native parsed object/warp counts differ.";
    }
    return {};
}

std::string check_runtime_case(const RuntimeMapCase& test){
    try{
        const JsonValue input = parse_json(map_document(test.input.fields));
        if (!test.expectation.normalized_fields.empty()){
            return check_pair(input, parse_json(map_document(test.expectation.normalized_fields)));
        }
        parse_map(input, "Maps/contract.json");
    }catch (const ParseException& error){
        const std::string message = error.message();
        if (!test.expectation.error.empty() && message.find(test.expectation.error) != std::string::npos && message.find("Maps/contract.json") != std::string::npos){
            return {};
        }
        return std::format("Unexpected parse error: {}", message);
    }
    return test.expectation.error.empty() ? "" : std::format("Expected rejection containing {}", test.expectation.error);
}

#ifndef PA_RUNTIME_MAP_STANDALONE
class RuntimeMapTest : public UnitTest{
public:
    explicit RuntimeMapTest(const RuntimeMapCase& test)
        : UnitTest(std::format("Navigation/RuntimeMap/{}", test.name)), m_test(test){}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string error = check_runtime_case(m_test);
        return error.empty() ? UnitTestResult(true) : UnitTestResult(error);
    }
private:
    const RuntimeMapCase& m_test;
};
#endif

} // namespace

#ifndef PA_RUNTIME_MAP_STANDALONE
void add_tests_RuntimeMap(UnitTestDatabase& database){
    for (const RuntimeMapCase& test : RUNTIME_MAP_CASES){
        database.add<RuntimeMapTest>(test);
    }
}
#endif

} // namespace Pokemon
} // namespace PokemonAutomation

#ifdef PA_RUNTIME_MAP_STANDALONE
// Optional argument: a parity pack of {input, expectation} records from a pinned publisher.
int main(int argc, char* argv[]){
    int failed = 0;
    for (const auto& test : PokemonAutomation::Pokemon::RUNTIME_MAP_CASES){
        const std::string error = PokemonAutomation::Pokemon::check_runtime_case(test);
        std::cout << test.name << ": " << (error.empty() ? "PASS" : error) << std::endl;
        failed += !error.empty();
    }
    if (argc == 2){
        const auto pack = PokemonAutomation::load_json_file(argv[1]);
        for (const auto& record : pack.to_array_throw()){
            const auto& object = record.to_object_throw();
            const std::string error = PokemonAutomation::Pokemon::check_pair(object.get_value_throw("input"), object.get_value_throw("expectation"));
            failed += !error.empty();
            if (!error.empty()){
                std::cerr << error << std::endl;
            }
        }
        std::cout << "Checked " << pack.to_array_throw().size() << " complete map records." << std::endl;
    }else if (argc != 1){
        return 2;
    }
    return failed;
}
#endif
