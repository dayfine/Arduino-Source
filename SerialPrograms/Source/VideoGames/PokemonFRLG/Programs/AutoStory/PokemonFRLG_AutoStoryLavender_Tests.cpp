/*  AutoStory Lavender Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows from 00.28 on (PokemonFRLG_AutoStoryTableLavender.cpp) on the
 *  overworld sim: 00.28 from Route 10's Pokemon Center through Rock Tunnel
 *  to its south mouth, and 00.29 down Route 10 to Lavender Town's Center.
 *  Each row whole, each open leg a tile short or long, and a wild battle
 *  on every open leg in the tunnel.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryResources.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryTable.h"
#include "PokemonFRLG_AutoStoryTableSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

struct AutoStoryLavenderInput{
    std::string name;
    std::function<UnitTestResult(Logger&)> body;
};

struct AutoStoryLavenderExpectation{
};

struct AutoStoryLavenderCase{
    AutoStoryLavenderInput input;
    AutoStoryLavenderExpectation expectation;
};

class Test_AutoStoryLavender : public UnitTest{
public:
    explicit Test_AutoStoryLavender(AutoStoryLavenderCase test_case)
        : UnitTest("PokemonFRLG::AutoStory - Simulation: " + test_case.input.name)
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
    AutoStoryLavenderCase m_case;
};

const StoryCheckpoint& checkpoint(std::string_view id){
    const std::vector<StoryCheckpoint>& rows = story_checkpoints();
    const auto row = std::find_if(rows.begin(), rows.end(), [&](const StoryCheckpoint& data){ return data.id == id; });
    if (row == rows.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No checkpoint {}.", id));
    }
    return *row;
}

//  The maps as 00.27 leaves them: the story's flags, Rock Tunnel's and
//  Route 10's trainers unfought.
std::vector<MapData> maps_after_route10(){
    const std::vector<MapData> all = overworld_maps();
    std::set<std::string> flags = map_flag_set(all, "after_hall_of_fame");
    flags.merge(map_flag_set(all, "new_game"));
    flags.erase("FLAG_HIDE_VERMILION_CITY_OAKS_AIDE");
    return overworld_maps(flags);
}

//  Row `id`'s steps from `first` on, on `sim` from `at`: the failure, or
//  empty once on the row's last route's target.
std::string row_rest(OverworldSim& sim, StoryPose at, const StoryCheckpoint& row, size_t first){
    std::string failure;
    for (size_t c = first; c < row.steps.size() && failure.empty(); c++){
        failure = row_step_on_sim(sim, at, row.steps[c], row.id);
    }
    if (!failure.empty()){
        return std::format("ended on {}; {}", to_string(sim.state().position), failure);
    }
    return "";
}

UnitTestResult row_checked(
    OverworldSim& sim, std::string_view id, const std::vector<std::vector<std::string>>& fought, const MapPosition& end
);

//  Row `id` on the sim with `script`: it ends on `end`, with no violation
//  on the way, having fought the trainers of one of `fought`, in order.
UnitTestResult row_on_sim(
    std::string_view id, const SimScript& script, const std::vector<std::vector<std::string>>& fought, const MapPosition& end
){
    const StoryCheckpoint& row = checkpoint(id);
    const StoryPose at = *story_pose(*row.start);
    OverworldSim sim(maps_after_route10(), MapPosition{at.map, at.tile}, *at.facing, script);
    return row_checked(sim, id, fought, end);
}

//  Row `id` on `sim`, from its start, checked as row_on_sim() does.
UnitTestResult row_checked(
    OverworldSim& sim, std::string_view id, const std::vector<std::vector<std::string>>& fought, const MapPosition& end
){
    const StoryCheckpoint& row = checkpoint(id);
    const StoryPose at = *story_pose(*row.start);
    if (const std::string failure = row_rest(sim, at, row, 0); !failure.empty()){
        return std::string(id) + ": " + failure;
    }
    if (std::find(fought.begin(), fought.end(), sim.state().trainers_beaten) == fought.end()){
        std::string fought;
        for (const std::string& name : sim.state().trainers_beaten){
            fought += " " + name;
        }
        return std::format("{}: fought{}.", id, fought);
    }
    if (!sim.state().violations.empty()){
        return std::string(id) + ": a violation on the way: " + sim.state().violations.back();
    }
    if (!(sim.state().position == end)){
        return std::format("{}: ended on {}, not {}.", id, to_string(sim.state().position), to_string(end));
    }
    return true;
}

const std::vector<std::string> ROCK_TUNNEL_TRAINERS{
    "poke_maniac", "poke_maniac", "picnicker", "hiker", "hiker", "picnicker", "hiker", "poke_maniac", "picnicker",
};

//  00.28 on the sim: the tree cut, the nine trainers fought where the
//  table steps into their lines (Cooper by the walker, on the leg past
//  him), no one else met, and the walk out of the south mouth.
UnitTestResult test_rock_tunnel_row(Logger&){
    return row_on_sim("00.28", SimScript{}, {ROCK_TUNNEL_TRAINERS}, MapPosition{StoryTable::ROUTE10, {8, 58}});
}

//  00.28 with a wild battle on the 3rd step on the tunnel's floor, mid-way
//  down the leg to the wall (17, 8): the walker runs, holds the leg to its
//  wall again, and nothing else changes.
UnitTestResult test_rock_tunnel_wild_battles(Logger&){
    SimScript script;
    script.encounters.push_back(SimEncounter{.grass_step = 3});
    return row_on_sim("00.28", script, {ROCK_TUNNEL_TRAINERS}, MapPosition{StoryTable::ROUTE10, {8, 58}});
}

//  Wild battles on the tunnel's floor, `gaps` steps apart in turn, from the
//  first step: as many as a row walks.
SimScript battles_every(const std::vector<int>& gaps){
    SimScript script;
    int step = 0;
    for (size_t c = 0; step < 3000; c++){
        step += gaps[c % gaps.size()];
        script.encounters.push_back(SimEncounter{.grass_step = step});
    }
    return script;
}

//  00.28's step `name`: where it is in the row.
size_t step_index(const StoryCheckpoint& row, std::string_view name){
    for (size_t c = 0; c < row.steps.size(); c++){
        if (row.steps[c].name == name){
            return c;
        }
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("No step {} in {}.", name, row.id));
}

//  Console B, 2026-11-09: after Winston, the walk west along B1F row 37 to
//  (13, 37) met a wild battle 10 to 14 tiles into every try. Each time the
//  walker found the player again at the east wall (45, 37), behind where
//  the leg started, and walked the whole 32 tiles again, until it gave up
//  ("stopped part-way 9 times"). From Winston's tile, with a battle every
//  10 to 14 steps, the steps from below_martha_line on reach Martha's line.
UnitTestResult test_b1f_row37_battles_every_12(Logger&){
    const StoryCheckpoint& row = checkpoint("00.28");
    const size_t first = step_index(row, "below_martha_line");
    StoryPose at{StoryTable::ROCK_TUNNEL_B1F, {29, 37}, Direction::NORTH};
    OverworldSim sim(maps_after_route10(), MapPosition{at.map, at.tile}, *at.facing, battles_every({12, 10, 14, 11, 13}));
    const std::string failure = row_step_on_sim(sim, at, row.steps[first], row.id);
    if (!failure.empty()){
        return std::format("below_martha_line: ended on {}; {}", to_string(sim.state().position), failure);
    }
    if (!(sim.state().position == MapPosition{StoryTable::ROCK_TUNNEL_B1F, {16, 35}})){
        return std::format("below_martha_line: ended on {}, not (16, 35).", to_string(sim.state().position));
    }
    return true;
}

//  From Winston's tile, below_martha_line, martha_line and Martha's battle
//  on `sim`: the failure, or empty once Martha alone is fought.
std::string winston_to_martha(OverworldSim& sim){
    const StoryCheckpoint& row = checkpoint("00.28");
    StoryPose at{StoryTable::ROCK_TUNNEL_B1F, {29, 37}, Direction::NORTH};
    for (size_t c = step_index(row, "below_martha_line"); c <= step_index(row, "picnicker_martha"); c++){
        if (const std::string failure = row_step_on_sim(sim, at, row.steps[c], row.id); !failure.empty()){
            return std::format("ended on {}; {}", to_string(sim.state().position), failure);
        }
    }
    if (sim.state().trainers_beaten != std::vector<std::string>{"picnicker"}){
        return std::format("ended on {}, Martha not fought.", to_string(sim.state().position));
    }
    return "";
}

//  Console B, 2026-11-09 (run 2): after Winston, the walk west along row
//  37 was found again at the wall (1, 38); a wild battle 0 to 2 tiles into
//  the walk east along row 38 left the player on one of (1..3, 38), and
//  the walk went on from there to the wall (19, 35). The one-tile step
//  north onto row 34 then ended short with nothing seen, the walk east
//  "to the wall (17, 34)" ran along row 35 to the rock (20, 35) instead
//  (the end frame shows the wall (19, 34) north, the rock east), and
//  martha_line found no trainer. Here the same battle, and the 10th hold
//  (that step, before the route went up column 16 into her line) a tile
//  short: Martha still sees the player.
UnitTestResult test_b1f_martha_after_row38_battle(Logger&){
    SimScript script;
    script.encounters = {SimEncounter{.grass_step = 31}};
    script.short_holds = {{1, 2}, {10, 1}};
    OverworldSim sim(maps_after_route10(), MapPosition{StoryTable::ROCK_TUNNEL_B1F, {29, 37}}, Direction::NORTH, script);
    if (const std::string failure = winston_to_martha(sim); !failure.empty()){
        return failure;
    }
    return true;
}

//  00.28 whole with a wild battle every 7 to 25 steps on the tunnel's floor,
//  the gaps drawn from each of these seeds: the row ends where it should.
UnitTestResult test_rock_tunnel_frequent_battles(Logger&){
    std::string failures;
    for (unsigned seed = 1; seed <= 12; seed++){
        std::mt19937 random(seed);
        std::uniform_int_distribution<int> gap(7, 25);
        std::vector<int> gaps;
        for (int c = 0; c < 400; c++){
            gaps.push_back(gap(random));
        }
        const UnitTestResult result = row_on_sim(
            "00.28", battles_every(gaps), {ROCK_TUNNEL_TRAINERS}, MapPosition{StoryTable::ROUTE10, {8, 58}}
        );
        if (result.result != UnitTestResult::PASSED){
            failures += std::format("{}Seed {}: {}", failures.empty() ? "" : " | ", seed, result.message);
        }
    }
    if (!failures.empty()){
        return failures;
    }
    return true;
}

//  The sim at row `id`'s start, with `script`.
OverworldSim row_start_sim(std::string_view id, SimScript script){
    const StoryPose at = *story_pose(*checkpoint(id).start);
    return OverworldSim(maps_after_route10(), MapPosition{at.map, at.tile}, *at.facing, std::move(script));
}

//  One run of a row on the sim, against the same row with no wild battle.
struct RowRunNumbers{
    bool passed = false;
    double seconds = 0;
    int tiles = 0;
    //  Tiles walked beyond the run with no wild battle: walked again after
    //  a battle, finding the player and the way back.
    int tiles_again = 0;
    int battles = 0;
    //  Per wild battle, the time beyond the run with no wild battle, less
    //  the battles' own: from each battle's end until the walk is back
    //  where it was cut off.
    double seconds_lost_per_battle = 0;
};

RowRunNumbers row_numbers(const SimState& run, bool passed, const SimState& clean){
    RowRunNumbers out;
    out.passed = passed;
    out.seconds = run.time.count() / 1000.;
    out.tiles = run.steps;
    out.tiles_again = run.steps - clean.steps;
    out.battles = run.wild_battles;
    const Milliseconds lost = run.time - clean.time - run.in_wild_battles;
    out.seconds_lost_per_battle = run.wild_battles > 0 ? lost.count() / 1000. / run.wild_battles : 0;
    return out;
}

//  The mean and 90th percentile of `values`.
std::string mean_p90(std::vector<double> values){
    if (values.empty()){
        return "-";
    }
    std::sort(values.begin(), values.end());
    const double mean = std::accumulate(values.begin(), values.end(), 0.) / values.size();
    const double p90 = values[std::min(values.size() - 1, (size_t)(0.9 * values.size()))];
    return std::format("mean {:.1f}, p90 {:.1f}", mean, p90);
}

//  How many of `runs` passed, and over those, the mean and 90th percentile
//  of each number.
std::string numbers_summary(const std::vector<RowRunNumbers>& runs){
    std::vector<double> seconds, tiles, again, battles, lost;
    size_t passed = 0;
    for (const RowRunNumbers& run : runs){
        passed += run.passed ? 1 : 0;
        if (!run.passed){
            continue;
        }
        seconds.push_back(run.seconds);
        tiles.push_back(run.tiles);
        again.push_back(run.tiles_again);
        battles.push_back(run.battles);
        if (run.battles > 0){
            lost.push_back(run.seconds_lost_per_battle);
        }
    }
    return std::format(
        "passed {}/{}; of those passed: time (s) {}; tiles {}; tiles walked again {}; wild battles {}; s lost per battle {}",
        passed, runs.size(), mean_p90(seconds), mean_p90(tiles), mean_p90(again), mean_p90(battles), mean_p90(lost)
    );
}

//  The land encounter rates of the maps 00.28 and 00.29 walk (pokefirered
//  src/data/wild_encounters.json, FireRed).
const std::map<std::string, int> ROCK_TUNNEL_ENCOUNTER_RATES{
    {StoryTable::ROCK_TUNNEL_1F, 7},
    {StoryTable::ROCK_TUNNEL_B1F, 7},
    {StoryTable::ROUTE10, 21},
};

constexpr unsigned GAME_ODDS_SEEDS = 30;
//  The share of turn taps the sim drops (SimScript::turn_tap_drop_rate):
//  a guess well above what the console showed, one in 00.28 run 2.
constexpr double TURN_TAP_DROP_RATE = 0.1;

//  00.28 whole, its wild battles at the game's own odds (OverworldSim::
//  rolls_encounter()) and `turn_tap_drop_rate` of its turn taps dropped,
//  from each of these seeds: every run ends where it should. Logs the
//  runs' numbers.
UnitTestResult rock_tunnel_game_odds(Logger& logger, double turn_tap_drop_rate){
    OverworldSim clean_sim = row_start_sim("00.28", SimScript{});
    if (const UnitTestResult clean = row_checked(clean_sim, "00.28", {ROCK_TUNNEL_TRAINERS}, MapPosition{StoryTable::ROUTE10, {8, 58}}); clean.result != UnitTestResult::PASSED){
        return clean;
    }
    std::vector<RowRunNumbers> runs;
    std::string failures;
    int dropped = 0;
    for (unsigned seed = 1; seed <= GAME_ODDS_SEEDS; seed++){
        SimScript script;
        script.encounter_rates = ROCK_TUNNEL_ENCOUNTER_RATES;
        script.encounter_seed = seed;
        script.turn_tap_drop_rate = turn_tap_drop_rate;
        script.turn_tap_seed = seed;
        OverworldSim sim = row_start_sim("00.28", script);
        const UnitTestResult result = row_checked(sim, "00.28", {ROCK_TUNNEL_TRAINERS}, MapPosition{StoryTable::ROUTE10, {8, 58}});
        const bool passed = result.result == UnitTestResult::PASSED;
        runs.push_back(row_numbers(sim.state(), passed, clean_sim.state()));
        dropped += sim.state().turn_taps_dropped;
        if (!passed){
            failures += std::format("{}Seed {}: {}", failures.empty() ? "" : " | ", seed, result.message);
        }
    }
    logger.log(std::format(
        "00.28 at the game's encounter odds, {} turn taps dropped: {}", dropped, numbers_summary(runs)
    ));
    if (!failures.empty()){
        return failures;
    }
    return true;
}

UnitTestResult test_rock_tunnel_game_odds(Logger& logger){
    return rock_tunnel_game_odds(logger, 0);
}

//  The same with one turn tap in TURN_TAP_DROP_RATE dropped by the game
//  (console B, 2026-11-09, 00.28 run 2).
UnitTestResult test_rock_tunnel_dropped_turn_taps(Logger& logger){
    return rock_tunnel_game_odds(logger, TURN_TAP_DROP_RATE);
}

//  00.29 on the sim: no trainer met, the ledge hopped, Lavender's nurse.
UnitTestResult test_lavender_row(Logger&){
    return row_on_sim("00.29", SimScript{}, {{}}, MapPosition{StoryTable::LAVENDER_CENTER, {7, 4}});
}

//  Where an open leg of two tiles or more ends a tile short or long, the
//  next leg starts a tile off. From each such start (an open tile), the
//  rest of the row still ends where the table says. A leg to a wall or
//  onto a warp ends on its tile, and a one-tile leg is one step.
UnitTestResult tile_off_after_open_legs(std::string_view id){
    const StoryCheckpoint& row = checkpoint(id);
    const std::vector<MapData>& maps = story_maps();
    std::optional<RouteStep> before;
    StoryPose at = *story_pose(*row.start);
    for (size_t s = 0; s < row.steps.size(); s++){
        const StoryStep& step = row.steps[s];
        const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS;
        if (!walks){
            //  A step that isn't a walk starts where the table says.
            before.reset();
            if (step.pose_after){
                at = *story_pose(*step.pose_after);
            }else if (step.kind == StoryStepKind::STEP && step.variant == CUT_TREE_VARIANT){
                at = StoryPose{at.map, at.tile.next(step.direction), step.direction};
            }
            continue;
        }
        const std::vector<RouteStep> route = walked_route(at, step).value_or(std::vector<RouteStep>{});
        MapPosition from{at.map, at.tile};
        for (size_t c = 0; c < route.size(); c++){
            const bool off = before && before->action == RouteAction::WALK && !before->to_wall && !before->warps && before->tiles > 1;
            const TilePos on = from.point.next(before ? before->direction : Direction::NORTH);
            const TilePos short_of{2 * from.point.x - on.x, 2 * from.point.y - on.y};
            const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == from.map; });
            for (const TilePos& start : off ? std::vector<TilePos>{short_of, on} : std::vector<TilePos>{}){
                const bool open = map != maps.end() && in_bounds(*map, start) && object_at(*map, start) == nullptr
                    && (tile_at(*map, start) == TileKind::OPEN || tile_at(*map, start) == TileKind::TALL_GRASS);
                if (!open){
                    continue;
                }
                OverworldSim sim(maps_after_route10(), MapPosition{from.map, start}, before->direction);
                const std::vector<RouteStep> rest(route.begin() + c, route.end());
                const RoutePlanning planning{.maps = &maps, .costs = story_route_costs(), .start = from, .label = step.name};
                std::string failure = OverworldSimWalker(sim).run(rest, planning);
                const StoryPose next{route.back().end.map, route.back().end.point, route.back().facing_after};
                failure = failure.empty() ? row_rest(sim, next, row, s + 1) : failure;
                if (!failure.empty()){
                    return std::format("{} {} leg {} ({}) from {}, not {}: {}", id, step.name, c + 1, to_string(route[c]), to_string(start), to_string(from.point), failure);
                }
            }
            before = route[c];
            from = route[c].end;
        }
        at = route.empty() ? at : StoryPose{route.back().end.map, route.back().end.point, route.back().facing_after};
    }
    return true;
}

UnitTestResult test_rock_tunnel_tile_off(Logger&){
    return tile_off_after_open_legs("00.28");
}

UnitTestResult test_lavender_tile_off(Logger&){
    return tile_off_after_open_legs("00.29");
}

//  00.29 saves in Lavender Town's Pokemon Center, LAVENDER TOWN on the
//  save card; 00.28 on Route 10.
UnitTestResult test_save_places(Logger&){
    if (save_card_place(StoryTable::LAVENDER_CENTER) != "lavender-town" || save_card_place(StoryTable::ROUTE10) != "route-10"){
        return "Wrong save card places for Lavender Town's Center or Route 10.";
    }
    return true;
}

} // namespace


const AutoStoryLavenderCase AUTO_STORY_LAVENDER_CASES[] = {
    {.input = {"00.28 the tree, Rock Tunnel's nine trainers, out of its south mouth", test_rock_tunnel_row}, .expectation = {}},
    {.input = {"00.28 a wild battle mid-leg in the tunnel", test_rock_tunnel_wild_battles}, .expectation = {}},
    {.input = {"00.28 a tile off after each open leg", test_rock_tunnel_tile_off}, .expectation = {}},
    {.input = {"00.28 B1F row 37 with a wild battle every 10 to 14 steps", test_b1f_row37_battles_every_12}, .expectation = {}},
    {.input = {"00.28 a wild battle every 7 to 25 steps, 12 seeds", test_rock_tunnel_frequent_battles}, .expectation = {}},
    {.input = {"00.28 wild battles at the game's odds, 30 seeds", test_rock_tunnel_game_odds}, .expectation = {}},
    {.input = {"00.28 wild battles at the game's odds and 1 in 10 turn taps dropped, 30 seeds", test_rock_tunnel_dropped_turn_taps}, .expectation = {}},
    {.input = {"00.28 B1F a wild battle into row 38, then a step short: Martha still sees the player", test_b1f_martha_after_row38_battle}, .expectation = {}},
    {.input = {"00.29 Route 10 south, the ledge, Lavender Town's Pokemon Center", test_lavender_row}, .expectation = {}},
    {.input = {"00.29 a tile off after each open leg", test_lavender_tile_off}, .expectation = {}},
    {.input = {"00.28 and 00.29 save on Route 10 and in Lavender Town", test_save_places}, .expectation = {}},
};

void add_tests_AutoStoryLavender(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_LAVENDER_CASES){
        database.add<Test_AutoStoryLavender>(item);
    }
    add_tests_AutoStoryResources(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
