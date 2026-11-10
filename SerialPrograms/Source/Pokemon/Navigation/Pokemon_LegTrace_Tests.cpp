/*  Leg Trace Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The leg record's line word for word, the route log's leg lines read
 *  back, and the tracer's records for a leg that ends, one cut off by the
 *  next, and one with a wild battle on the way.
 *
 */

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_LegTrace.h"
#include "Pokemon_Navigation_Tests.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

struct LegTraceInput{
    std::string name;
    std::function<std::string()> actual;
};
struct LegTraceExpectation{
    std::string expected;
};
struct LegTraceTestCase{
    LegTraceInput input;
    LegTraceExpectation expectation;
};

class Test_LegTrace : public UnitTest{
public:
    explicit Test_LegTrace(LegTraceTestCase scenario)
        : UnitTest("Pokemon::LegTrace - " + scenario.input.name)
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const std::string actual = m_scenario.input.actual();
        if (actual == m_scenario.expectation.expected){
            return true;
        }
        return "expected:\n" + m_scenario.expectation.expected + "\ngot:\n" + actual;
    }

private:
    LegTraceTestCase m_scenario;
};

std::string tiles_text(const std::vector<RouteTile>& tiles){
    return tiles.empty() ? "none" : describe(tiles);
}

std::string plan_text(const std::optional<LegPlan>& plan){
    if (!plan){
        return "none";
    }
    return std::string(direction_name(plan->direction)) + " " + std::to_string(plan->tiles) + " "
        + leg_end_kind_name(plan->end_kind) + " -> " + tiles_text(plan->ends);
}

//  The lines a tracer emits for `script`, one a line.
std::string traced(const std::function<void(LegTracer&)>& script){
    std::string out;
    LegTracer tracer([&](const std::string& line){ out += line + "\n"; });
    script(tracer);
    return out;
}

const LegTraceTestCase CASES[] = {
    {
        .input = {.name = "a tap leg's line", .actual = []{
            LegRecord record{
                .checkpoint = "00.04", .event = "to_the_signs_row", .leg = 1,
                .plan = {.direction = Direction::NORTH, .tiles = 1, .end_kind = LegEndKind::TILE,
                    .ends = {{"Route 201", {57, 4}}}},
                .moved = 1, .chunks = 1, .walked_ms = 800,
                .end = {{"Route 201", {57, 4}}}, .known = LegEndKnown::COUNTED,
            };
            return leg_trace_line(record);
        }},
        .expectation = {.expected =
            "[Leg] {\"checkpoint\":\"00.04\",\"chunks\":1,\"dir\":\"north\",\"end\":[[\"Route 201\",57,4]],"
            "\"end_kind\":\"tile\",\"event\":\"to_the_signs_row\",\"interrupts\":[],\"known\":\"counted\","
            "\"leg\":1,\"moved\":1,\"ms\":800,\"planned_end\":[[\"Route 201\",57,4]],\"tiles\":1}"},
    },
    {
        .input = {.name = "tiles read back", .actual = []{
            return tiles_text(parse_route_tiles("Route 202 (23, 25), Mom's House (30, -1)"));
        }},
        .expectation = {.expected = "Route 202 (23, 25), Mom's House (30, -1)"},
    },
    {
        .input = {.name = "no tiles in other text", .actual = []{
            return tiles_text(parse_route_tiles("an unknown tile"));
        }},
        .expectation = {.expected = "none"},
    },
    {
        .input = {.name = "a wall leg line read back", .actual = []{
            return plan_text(parse_route_leg_line("Leg 1/2: east 15 (to wall) to Route 202 (23, 25), Route 202 (30, 26)"));
        }},
        .expectation = {.expected = "east 15 wall -> Route 202 (23, 25), Route 202 (30, 26)"},
    },
    {
        .input = {.name = "an open leg line read back", .actual = []{
            return plan_text(parse_route_leg_line("Leg 3/3: west 1 (open) to Twinleaf Town (7, 9)"));
        }},
        .expectation = {.expected = "west 1 open -> Twinleaf Town (7, 9)"},
    },
    {
        .input = {.name = "not a leg line", .actual = []{
            return plan_text(parse_route_leg_line("[Route] Turning west."));
        }},
        .expectation = {.expected = "none"},
    },
    {
        .input = {.name = "a leg cut off by the next, and by its event's end", .actual = []{
            return traced([](LegTracer& tracer){
                tracer.begin_event("00.04", "beside_mom");
                tracer.begin_leg({.direction = Direction::WEST, .tiles = 2, .end_kind = LegEndKind::OPEN});
                tracer.begin_leg({.direction = Direction::NORTH, .tiles = 1, .end_kind = LegEndKind::WALL});
                tracer.walked({.chunks = 3, .walked_ms = 640});
                tracer.begin_event("00.04", "mom");
            });
        }},
        .expectation = {.expected =
            "[Leg] {\"checkpoint\":\"00.04\",\"chunks\":0,\"dir\":\"west\",\"end\":[],\"end_kind\":\"open\","
            "\"event\":\"beside_mom\",\"interrupts\":[],\"known\":\"unknown\",\"leg\":1,\"ms\":0,\"planned_end\":[],\"tiles\":2}\n"
            "[Leg] {\"checkpoint\":\"00.04\",\"chunks\":3,\"dir\":\"north\",\"end\":[],\"end_kind\":\"wall\","
            "\"event\":\"beside_mom\",\"interrupts\":[],\"known\":\"unknown\",\"leg\":2,\"ms\":640,\"planned_end\":[],\"tiles\":1}\n"},
    },
    {
        .input = {.name = "a wild battle on the way, two walks to the wall", .actual = []{
            return traced([](LegTracer& tracer){
                tracer.begin_event("00.04b", "to_catch_grass");
                tracer.begin_leg({.direction = Direction::SOUTH, .tiles = 4, .end_kind = LegEndKind::WALL});
                tracer.walked({.chunks = 2, .walked_ms = 500, .wild_battles = 1});
                tracer.walked({.chunks = 2, .walked_ms = 400});
                tracer.end_leg({{"Route 202", {20, 27}}}, LegEndKnown::WALL);
                tracer.end_event();
            });
        }},
        .expectation = {.expected =
            "[Leg] {\"checkpoint\":\"00.04b\",\"chunks\":4,\"dir\":\"south\",\"end\":[[\"Route 202\",20,27]],\"end_kind\":\"wall\","
            "\"event\":\"to_catch_grass\",\"interrupts\":[\"wild_battle\"],\"known\":\"wall\",\"leg\":1,\"ms\":900,\"planned_end\":[],\"tiles\":4}\n"},
    },
};

} // namespace


void add_tests_LegTrace(UnitTestDatabase& database){
    for (const auto& scenario : CASES){
        database.add<Test_LegTrace>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
