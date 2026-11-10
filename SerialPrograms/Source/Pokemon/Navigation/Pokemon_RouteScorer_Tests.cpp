/*  Route Scorer Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The scorer on small made-up grids.
 *
 */

#include <format>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon_Navigation_Tests.h"
#include "Pokemon_RouteScorer.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  '#' wall, '.' floor, '"' tall grass, 'D' a warp, 'T' a scene's trigger,
//  'w' a wanderer's tile, 's' a trainer's sight, 'v' a ledge hopped south.
class ToyMap{
public:
    explicit ToyMap(std::vector<std::string> rows)
        : m_rows(std::move(rows))
    {}

    char at(const TilePos& pos) const{
        if (pos.y < 0 || pos.y >= (int)m_rows.size() || pos.x < 0 || pos.x >= (int)m_rows[pos.y].size()){
            return '#';
        }
        return m_rows[pos.y][pos.x];
    }
    ScoreMap score_map() const{
        return {
            .step = [this](const TilePos& from, Direction direction) -> std::optional<TilePos>{
                const TilePos next = from.next(direction);
                if (at(next) == 'v'){
                    return direction == Direction::SOUTH ? std::optional<TilePos>(next.next(direction)) : std::nullopt;
                }
                return at(next) == '#' ? std::nullopt : std::optional<TilePos>(next);
            },
            .risk = [this](const TilePos& pos){
                switch (at(pos)){
                case '"':
                    return TileRisk::ENCOUNTER;
                case 'D':
                    return TileRisk::WARP;
                case 'T':
                    return TileRisk::TRIGGER;
                case 'w':
                    return TileRisk::WANDERER;
                case 's':
                    return TileRisk::SIGHT;
                default:
                    return TileRisk::NONE;
                }
            },
        };
    }

private:
    std::vector<std::string> m_rows;
};

RoutePaces toy_paces(){
    return {
        .ms_per_tile = [](Direction, bool run){ return run ? 100LL : 250LL; },
        .start_ms = 250, .tap_ms = 800, .press_ms = 100, .stop_ms = 1000,
        .open_hold_error = [](size_t tiles){ return 1 + tiles / 5; },
    };
}

InterruptModel toy_model(){
    InterruptModel model;
    model.encounter_per_tile = 0.1;
    model.encounter_ms = 12000;
    model.wanderer_per_tile = 0.25;
    model.wanderer_ms = 2500;
    model.trainer_ms = 45000;
    model.replan_ms = 2000;
    return model;
}

std::function<bool(const TilePos&)> exactly(const TilePos& goal){
    return [goal](const TilePos& pos){ return pos == goal; };
}

RouteScore score(const ToyMap& toy, const std::vector<TilePos>& from, const TilePos& goal,
    const std::vector<ScoredLeg>& legs, const RouteReplanner& replan = nullptr, const InterruptModel& model = toy_model()
){
    return score_route(toy.score_map(), from, exactly(goal), legs, toy_paces(), model, replan);
}

//  "" if `got` has `verdict`, else why not.
std::string expect(const RouteScore& got, RouteVerdict verdict, const std::string& what){
    if (got.verdict != verdict){
        return std::format("{}: expected {}, got {} ({})", what, verdict_name(verdict), verdict_name(got.verdict), got.why);
    }
    return "";
}

//  Passes if every check is "".
UnitTestResult all_of(std::initializer_list<std::string> checks){
    for (const std::string& check : checks){
        if (!check.empty()){
            return check;
        }
    }
    return true;
}

UnitTestResult test_taps_time(){
    const ToyMap toy({"......"});
    const RouteScore got = score(toy, {{0, 0}}, {4, 0}, {{.direction = Direction::EAST, .tiles = 4, .kind = ScoredLegKind::TAPS}});
    if (got.verdict != RouteVerdict::PASS || got.nominal_ms != 4 * 800 + 100 || got.presses != 4 || got.stops != 0){
        return std::format("4 taps: {} {} ms, {} presses, {} stops", verdict_name(got.verdict), got.nominal_ms, got.presses, got.stops);
    }
    return true;
}

UnitTestResult test_run_to_wall_beats_taps(){
    const ToyMap toy({"......#"});
    const RouteScore taps = score(toy, {{0, 0}}, {5, 0}, {{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}});
    const RouteScore run = score(toy, {{0, 0}}, {5, 0}, {{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TO_WALL, .run = true}});
    if (run.verdict != RouteVerdict::PASS || run.nominal_ms != 250 + 500 + 1000 + 100 || run.stops != 1){
        return std::format("run: {} {} ms ({})", verdict_name(run.verdict), run.nominal_ms, run.why);
    }
    return run.expected_ms < taps.expected_ms ? UnitTestResult(true) : UnitTestResult("the run is not faster than the taps");
}

UnitTestResult test_wall_collapses_a_set(){
    //  Three columns, all stopped by row 2's wall, then taps west.
    const ToyMap toy({".....", ".....", "#####"});
    const RouteScore got = score(toy, {{2, 0}}, {0, 1}, {
        {.direction = Direction::EAST, .tiles = 2, .kind = ScoredLegKind::OPEN_HOLD, .run = true},
        {.direction = Direction::SOUTH, .tiles = 1, .kind = ScoredLegKind::TO_WALL},
        {.direction = Direction::WEST, .tiles = 4, .kind = ScoredLegKind::TO_WALL},
    });
    return all_of({expect(got, RouteVerdict::PASS, "open run east, then walls south and west")});
}

UnitTestResult test_open_hold_off_the_goal(){
    const ToyMap toy({"........"});
    const RouteScore got = score(toy, {{0, 0}}, {3, 0}, {{.direction = Direction::EAST, .tiles = 3, .kind = ScoredLegKind::OPEN_HOLD}});
    return all_of({expect(got, RouteVerdict::FAIL, "an open hold with nothing to stop it")});
}

UnitTestResult test_open_hold_into_grass(){
    const ToyMap toy({"....\"\"\"#"});
    const RouteScore got = score(toy, {{0, 0}}, {6, 0}, {
        {.direction = Direction::EAST, .tiles = 3, .kind = ScoredLegKind::OPEN_HOLD},
        {.direction = Direction::EAST, .tiles = 3, .kind = ScoredLegKind::TO_WALL},
    });
    return all_of({expect(got, RouteVerdict::FAIL, "an open hold whose overshoot reaches grass")});
}

UnitTestResult test_open_hold_onto_a_ledge(){
    const ToyMap toy({".", ".", ".", "v", ".", "#"});
    const RouteScore got = score(toy, {{0, 0}}, {0, 4}, {
        {.direction = Direction::SOUTH, .tiles = 2, .kind = ScoredLegKind::OPEN_HOLD},
        {.direction = Direction::SOUTH, .tiles = 2, .kind = ScoredLegKind::TO_WALL},
    });
    return all_of({expect(got, RouteVerdict::FAIL, "an open hold whose overshoot hops a ledge")});
}

UnitTestResult test_open_hold_before_a_wall(){
    const ToyMap toy({".....#"});
    const RouteScore got = score(toy, {{0, 0}}, {4, 0}, {{.direction = Direction::EAST, .tiles = 4, .kind = ScoredLegKind::OPEN_HOLD, .run = true}});
    return all_of({
        expect(got, RouteVerdict::FAIL, "an open hold that may stop a tile short"),
        expect(score(toy, {{0, 0}}, {4, 0}, {{.direction = Direction::EAST, .tiles = 3, .kind = ScoredLegKind::OPEN_HOLD, .run = true},
            {.direction = Direction::EAST, .tiles = 2, .kind = ScoredLegKind::TO_WALL, .run = true}}),
            RouteVerdict::PASS, "an open hold that overshoots only toward a wall, then the wall"),
    });
}

UnitTestResult test_wall_leg_through_a_warp(){
    const ToyMap toy({"..D..#"});
    const RouteScore got = score(toy, {{0, 0}}, {4, 0}, {{.direction = Direction::EAST, .tiles = 4, .kind = ScoredLegKind::TO_WALL}});
    return all_of({
        expect(got, RouteVerdict::FAIL, "a hold that walks into a warp"),
        expect(score(toy, {{0, 0}}, {2, 0}, {{.direction = Direction::EAST, .tiles = 2, .kind = ScoredLegKind::TO_WALL}}),
            RouteVerdict::PASS, "a hold that ends in the warp it is for"),
    });
}

UnitTestResult test_trigger_stops_a_run(){
    const ToyMap toy({"..T...."});
    return all_of({expect(score(toy, {{0, 0}}, {2, 0}, {{.direction = Direction::EAST, .tiles = 2, .kind = ScoredLegKind::TO_WALL, .run = true}}),
        RouteVerdict::PASS, "a run that the goal's trigger stops")});
}

UnitTestResult test_grass_charges_interruptions(){
    const ToyMap toy({".\"\"\"..#"});
    const RouteScore got = score(toy, {{0, 0}}, {5, 0}, {{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TO_WALL}});
    if (got.verdict != RouteVerdict::PASS || got.recoveries != 3 || got.replans != 0){
        return std::format("{}: {} recoveries, {} replans ({})", verdict_name(got.verdict), got.recoveries, got.replans, got.why);
    }
    if (got.expected_ms <= (double)got.nominal_ms || got.worst_ms <= got.nominal_ms){
        return std::format("expected {} ms, worst {} ms, nominal {} ms", got.expected_ms, got.worst_ms, got.nominal_ms);
    }
    return true;
}

UnitTestResult test_taps_in_grass_need_a_replan(){
    //  A lost tap in the grass leaves us a tile either way; the taps after it
    //  cannot tell, so without a replanner the plan fails.
    const ToyMap toy({".\"\"\"...."});
    const std::vector<ScoredLeg> taps{{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}};
    const RouteScore failed = score(toy, {{0, 0}}, {5, 0}, taps);
    //  A replanner that goes east to the wall absorbs it.
    const ToyMap walled({".\"\"\"..#"});
    const RouteReplanner to_wall = [](const std::vector<TilePos>&){
        return std::optional<std::vector<ScoredLeg>>({{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TO_WALL}});
    };
    const RouteScore replanned = score(walled, {{0, 0}}, {5, 0}, taps, to_wall);
    if (replanned.replans != 3){
        return std::format("{} replans", replanned.replans);
    }
    return all_of({
        expect(failed, RouteVerdict::FAIL, "taps in grass without a replanner"),
        expect(replanned, RouteVerdict::PASS, "taps in grass with a wall to replan to"),
    });
}

UnitTestResult test_recovery_back_through_the_grass_strands(){
    //  The recovery walks back to the west wall, still in the grass, and
    //  taps east through it: an interruption there leads to the same
    //  recovery again, from tiles never known.
    const ToyMap toy({"#\"\"\"\"...."});
    const std::vector<ScoredLeg> taps{{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}};
    const RouteReplanner back = [](const std::vector<TilePos>&){
        return std::optional<std::vector<ScoredLeg>>({{.direction = Direction::WEST, .tiles = 4, .kind = ScoredLegKind::TO_WALL},
            {.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}});
    };
    return all_of({expect(score(toy, {{1, 0}}, {6, 0}, taps, back), RouteVerdict::STRAND, "a recovery back through the grass")});
}

UnitTestResult test_recovery_from_a_wall_out_of_the_grass_retries(){
    //  The same, but the west wall stops us on a tile out of the grass: the
    //  recovery starts from a known tile each time, so an interruption on
    //  the way back is a retry that ends, not a strand (00.04b
    //  to_catch_grass, whose goal is in the grass).
    const ToyMap toy({"#.\"\"\"...."});
    const std::vector<ScoredLeg> taps{{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}};
    const RouteReplanner back = [](const std::vector<TilePos>&){
        return std::optional<std::vector<ScoredLeg>>({{.direction = Direction::WEST, .tiles = 4, .kind = ScoredLegKind::TO_WALL},
            {.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}});
    };
    return all_of({expect(score(toy, {{1, 0}}, {6, 0}, taps, back), RouteVerdict::PASS, "a recovery from a wall out of the grass")});
}

UnitTestResult test_too_many_recoveries(){
    const ToyMap toy({".\"\"\"..#"});
    const RouteReplanner to_wall = [](const std::vector<TilePos>&){
        return std::optional<std::vector<ScoredLeg>>({{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TO_WALL}});
    };
    InterruptModel model = toy_model();
    model.max_recoveries = 0;
    return all_of({expect(score(toy, {{0, 0}}, {5, 0}, {{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TAPS}}, to_wall, model),
        RouteVerdict::RETRIES, "a replan with no recoveries allowed")});
}

UnitTestResult test_trainer_sight_once(){
    const ToyMap toy({".ss...#"});
    const RouteScore got = score(toy, {{0, 0}}, {5, 0}, {{.direction = Direction::EAST, .tiles = 5, .kind = ScoredLegKind::TO_WALL}});
    if (got.recoveries != 1 || got.expected_ms < (double)(got.nominal_ms + 45000)){
        return std::format("{} recoveries, expected {} ms", got.recoveries, got.expected_ms);
    }
    return true;
}


struct RouteScorerInput{
    std::string name;
    UnitTestResult (*test)();
};

struct RouteScorerExpectation{
    std::string_view description;
};

struct RouteScorerCase{
    RouteScorerInput input;
    RouteScorerExpectation expectation;
};

class Test_RouteScorer : public UnitTest{
public:
    explicit Test_RouteScorer(RouteScorerCase scenario)
        : UnitTest("Pokemon::Navigation - RouteScorer: " + scenario.input.name)
        , m_case(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.test();
    }

private:
    RouteScorerCase m_case;
};

const RouteScorerCase ROUTE_SCORER_CASES[] = {
    {
        .input = {.name = "taps take a tap each", .test = test_taps_time},
        .expectation = {.description = "4 taps cost 4 taps and a press"},
    },
    {
        .input = {.name = "a run to a wall beats taps", .test = test_run_to_wall_beats_taps},
        .expectation = {.description = "run to the wall is faster than the same tiles in taps"},
    },
    {
        .input = {.name = "walls bring a set of tiles to one", .test = test_wall_collapses_a_set},
        .expectation = {.description = "PASS"},
    },
    {
        .input = {.name = "an open hold with nothing to stop it fails", .test = test_open_hold_off_the_goal},
        .expectation = {.description = "FAIL: may end off the goal"},
    },
    {
        .input = {.name = "an open hold may not overshoot into grass", .test = test_open_hold_into_grass},
        .expectation = {.description = "FAIL: overshoot onto grass"},
    },
    {
        .input = {.name = "an open hold may not overshoot onto a ledge", .test = test_open_hold_onto_a_ledge},
        .expectation = {.description = "FAIL: overshoot over a ledge"},
    },
    {
        .input = {.name = "an open hold may overshoot toward a wall", .test = test_open_hold_before_a_wall},
        .expectation = {.description = "FAIL when short of the goal, PASS with the wall after it"},
    },
    {
        .input = {.name = "a hold may not walk into a warp", .test = test_wall_leg_through_a_warp},
        .expectation = {.description = "FAIL through a warp, PASS into the goal warp"},
    },
    {
        .input = {.name = "the goal's trigger stops a run", .test = test_trigger_stops_a_run},
        .expectation = {.description = "PASS"},
    },
    {
        .input = {.name = "grass charges interruptions", .test = test_grass_charges_interruptions},
        .expectation = {.description = "3 recoveries, expected and worst above nominal"},
    },
    {
        .input = {.name = "taps in grass need a replan", .test = test_taps_in_grass_need_a_replan},
        .expectation = {.description = "FAIL without a replanner, PASS with 3 replans"},
    },
    {
        .input = {.name = "a recovery back through the grass strands", .test = test_recovery_back_through_the_grass_strands},
        .expectation = {.description = "STRAND"},
    },
    {
        .input = {.name = "a recovery from a wall out of the grass retries", .test = test_recovery_from_a_wall_out_of_the_grass_retries},
        .expectation = {.description = "PASS"},
    },
    {
        .input = {.name = "recoveries in a row are capped", .test = test_too_many_recoveries},
        .expectation = {.description = "RETRIES"},
    },
    {
        .input = {.name = "a trainer sees us once", .test = test_trainer_sight_once},
        .expectation = {.description = "one trainer battle charged"},
    },
};

} // namespace


void add_tests_RouteScorer(UnitTestDatabase& database){
    for (const auto& scenario : ROUTE_SCORER_CASES){
        database.add<Test_RouteScorer>(scenario);
    }
}



} // namespace Pokemon
} // namespace PokemonAutomation
