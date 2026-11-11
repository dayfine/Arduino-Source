/*  AutoStory Lavender Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Rows from 08.01 on (PokemonFRLG_AutoStoryTableLavender.cpp) on the
 *  overworld sim: 08.01 from Route 10's Pokemon Center through Rock Tunnel
 *  to its south mouth, 08.02 down Route 10 to Lavender Town's Center, and
 *  08.03 west over Route 8 and under Saffron to Celadon City.
 *  Each row whole, each open leg a tile short or long, and a wild battle
 *  on every open leg in the tunnel.
 *
 */

#include <algorithm>
#include <format>
#include <functional>
#include <map>
#include <memory>
#include <numeric>
#include <optional>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_BuiltinStoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_HideoutLiftRoom.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryLegs.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryFaultSweep_Tests.h"
#include "PokemonFRLG_AutoStoryResources.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRows_Tests.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string LAVENDER_CENTER = "LavenderTown_PokemonCenter_1F";
const std::string ROCK_TUNNEL_1F = "RockTunnel_1F";
const std::string ROCK_TUNNEL_B1F = "RockTunnel_B1F";
const std::string ROUTE10 = "Route10";

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

//  The maps as 07.02 leaves them: the story's flags, Rock Tunnel's and
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
    const StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(maps_after_route10(), MapPosition{at.map, at.tile}, *at.facing, script);
    return row_checked(sim, id, fought, end);
}

//  Row `id` on `sim`, from its start, checked as row_on_sim() does.
UnitTestResult row_checked(
    OverworldSim& sim, std::string_view id, const std::vector<std::vector<std::string>>& fought, const MapPosition& end
){
    const StoryCheckpoint& row = checkpoint(id);
    const StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
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

//  08.01 on the sim: the tree cut, the nine trainers fought where the
//  table steps into their lines (Cooper by the walker, on the leg past
//  him), no one else met, and the walk out of the south mouth.
UnitTestResult test_rock_tunnel_row(Logger&){
    return row_on_sim("08.01", SimScript{}, {ROCK_TUNNEL_TRAINERS}, MapPosition{ROUTE10, {8, 58}});
}

//  08.01 with a wild battle on the 3rd step on the tunnel's floor, mid-way
//  down the leg to the wall (17, 8): the walker runs, holds the leg to its
//  wall again, and nothing else changes.
UnitTestResult test_rock_tunnel_wild_battles(Logger&){
    SimScript script;
    script.encounters.push_back(SimEncounter{.grass_step = 3});
    return row_on_sim("08.01", script, {ROCK_TUNNEL_TRAINERS}, MapPosition{ROUTE10, {8, 58}});
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

//  08.01's step `name`: where it is in the row.
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
    const StoryCheckpoint& row = checkpoint("08.01");
    const size_t first = step_index(row, "below_martha_line");
    StoryPose at{ROCK_TUNNEL_B1F, {29, 37}, Direction::NORTH};
    OverworldSim sim(maps_after_route10(), MapPosition{at.map, at.tile}, *at.facing, battles_every({12, 10, 14, 11, 13}));
    const std::string failure = row_step_on_sim(sim, at, row.steps[first], row.id);
    if (!failure.empty()){
        return std::format("below_martha_line: ended on {}; {}", to_string(sim.state().position), failure);
    }
    if (!(sim.state().position == MapPosition{ROCK_TUNNEL_B1F, {16, 35}})){
        return std::format("below_martha_line: ended on {}, not (16, 35).", to_string(sim.state().position));
    }
    return true;
}

//  From Winston's tile, below_martha_line, martha_line and Martha's battle
//  on `sim`: the failure, or empty once Martha alone is fought.
std::string winston_to_martha(OverworldSim& sim){
    const StoryCheckpoint& row = checkpoint("08.01");
    StoryPose at{ROCK_TUNNEL_B1F, {29, 37}, Direction::NORTH};
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
    OverworldSim sim(maps_after_route10(), MapPosition{ROCK_TUNNEL_B1F, {29, 37}}, Direction::NORTH, script);
    if (const std::string failure = winston_to_martha(sim); !failure.empty()){
        return failure;
    }
    return true;
}

//  Regression: console B, 2026-11-09 15:50, 08.01 winston_line, dark: from
//  (32, 37) west into Winston's line, a wild battle on the 2nd tap's tile
//  (30, 37), run from; the 3rd tap, sent as the battle's black ended, was
//  lost, and the step failed no_trainer. After the battle the next tap
//  waits for the overworld (wait_out_battle_exit()): Winston sees the
//  player on (29, 37).
UnitTestResult test_winston_line_battle_on_tap_2(Logger&){
    const StoryCheckpoint& row = checkpoint("08.01");
    SimScript script;
    script.dark_maps = {ROCK_TUNNEL_1F, ROCK_TUNNEL_B1F};
    script.wild_floor_maps = {ROCK_TUNNEL_B1F};
    script.encounters = {SimEncounter{.grass_step = 2}};
    StoryPose at{ROCK_TUNNEL_B1F, {32, 37}, Direction::WEST};
    OverworldSim sim(maps_after_route10(), MapPosition{at.map, at.tile}, Direction::WEST, script);
    const std::string failure = row_step_on_sim(sim, at, row.steps[step_index(row, "winston_line")], row.id);
    const auto lost = std::find_if(sim.state().log.begin(), sim.state().log.end(), [](const std::string& line){
        return line.find("in the lockout after a battle") != std::string::npos;
    });
    if (!failure.empty() || lost != sim.state().log.end() || sim.state().wild_battles != 1){
        return std::format(
            "winston_line: ended on {}, {} wild battle(s), {}; {}", to_string(sim.state().position),
            sim.state().wild_battles, lost == sim.state().log.end() ? "no tap lost" : *lost, failure
        );
    }
    return true;
}

//  Rock Tunnel has no Flash: both floors are dark.
const std::set<std::string> ROCK_TUNNEL_DARK{ROCK_TUNNEL_1F, ROCK_TUNNEL_B1F};

//  Regression: console B, 2026-11-09, 08.01 hiker_lenny: won, and the dark
//  overworld after it was taken for an evolution until fight_battle() gave
//  up. 08.01 dark: every trainer's battle ends on the dark cave's overworld.
UnitTestResult test_rock_tunnel_dark_battle_ends(Logger&){
    SimScript script;
    script.dark_maps = ROCK_TUNNEL_DARK;
    return row_on_sim("08.01", script, {ROCK_TUNNEL_TRAINERS}, MapPosition{ROUTE10, {8, 58}});
}

//  below_martha_line as `legs` from Winston's tile (29, 37) on `sim`, by the
//  walker with `rules`: the failure, or empty if it ended on (16, 35).
std::string walk_below_martha(OverworldSim& sim, const std::vector<StoryLeg>& legs, const SimWalkerRules& rules){
    const MapPosition start{ROCK_TUNNEL_B1F, {29, 37}};
    const std::vector<MapData>& maps = story_maps();
    const std::optional<std::vector<RouteStep>> route = fixed_route(maps, start, legs);
    if (!route){
        return "the legs don't fit the map";
    }
    const RoutePlanning planning{.maps = &maps, .costs = story_route_costs(), .start = start, .label = "below_martha_line"};
    std::string failure = OverworldSimWalker(sim, rules).run(*route, planning);
    if (failure.empty() && !(sim.state().position == MapPosition{ROCK_TUNNEL_B1F, {16, 35}})){
        failure = "ended on " + to_string(sim.state().position) + ", not (16, 35)";
    }
    return failure;
}

//  Console B, 2026-11-09 13:35, below_martha_line as it was then: west 16,
//  north 2, east 6 to the wall (19, 35), west 3. A battle 6 tiles into
//  west 16; the player found again at (1, 38); a battle seen late at the
//  end of the walk back east 12 along row 38, placed 0 to 3 tiles in by
//  the dark screen; "found again" at the rock (20, 35) while against the
//  wall (25, 35); ended on (21, 35), where martha_line found no trainer.
//  Here the second battle on each step of that walk back: with the dark
//  cave's stops placed anywhere on their leg, every one ends on (16, 35);
//  without, some don't.
UnitTestResult test_b1f_dark_row38_battle_old_legs(Logger& logger){
    const std::vector<StoryLeg> old_legs{{Direction::WEST, 16}, {Direction::NORTH, 2}, {Direction::EAST, 6}, {Direction::WEST, 3}};
    std::string failures;
    int old_wrong = 0;
    for (int second = 8; second <= 60; second++){
        SimScript script;
        script.dark_maps = ROCK_TUNNEL_DARK;
        script.encounters = {SimEncounter{.grass_step = 6}, SimEncounter{.grass_step = second, .seen_late = true}};
        OverworldSim fixed(maps_after_route10(), MapPosition{ROCK_TUNNEL_B1F, {29, 37}}, Direction::NORTH, script);
        if (const std::string failure = walk_below_martha(fixed, old_legs, SimWalkerRules{}); !failure.empty()){
            failures += std::format("{}battle on step {}: {}", failures.empty() ? "" : " | ", second, failure);
        }
        SimWalkerRules old_rules;
        old_rules.whole_leg_in_dark = false;
        old_rules.dark_cave_not_battle = false;
        OverworldSim old(maps_after_route10(), MapPosition{ROCK_TUNNEL_B1F, {29, 37}}, Direction::NORTH, script);
        if (const std::string failure = walk_below_martha(old, old_legs, old_rules); !failure.empty()){
            old_wrong++;
            logger.log(std::format("Without the dark cave rules, battle on step {}: {}", second, failure));
        }
    }
    if (!failures.empty()){
        return failures;
    }
    if (old_wrong == 0){
        return "Without the dark cave rules every walk still ended on (16, 35): the test doesn't show the failure.";
    }
    return true;
}

//  Rock Tunnel dark, from Winston's tile: below_martha_line, martha_line
//  and Martha's battle with one wild battle on each step the walk takes,
//  on rows 37 and 38 and on row 35 between the corner (11, 35) and
//  (16, 35), seen as it starts and seen late: Martha alone is fought.
UnitTestResult test_b1f_dark_martha_battle_anywhere(Logger&){
    std::string failures;
    for (int step = 1; step <= 40; step++){
        for (const bool late : {false, true}){
            SimScript script;
            script.dark_maps = ROCK_TUNNEL_DARK;
            script.encounters = {SimEncounter{.grass_step = step, .seen_late = late}};
            OverworldSim sim(maps_after_route10(), MapPosition{ROCK_TUNNEL_B1F, {29, 37}}, Direction::NORTH, script);
            if (const std::string failure = winston_to_martha(sim); !failure.empty()){
                failures += std::format("{}step {}{}: {}", failures.empty() ? "" : " | ", step, late ? " late" : "", failure);
            }
        }
    }
    if (!failures.empty()){
        return failures;
    }
    return true;
}

//  08.01 whole with a wild battle every 7 to 25 steps on the tunnel's floor,
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
            "08.01", battles_every(gaps), {ROCK_TUNNEL_TRAINERS}, MapPosition{ROUTE10, {8, 58}}
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
    const StoryPose at = *story_pose(Pokemon::pose_of(*checkpoint(id).start));
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

//  The land encounter rates of the maps 08.01 and 08.02 walk (pokefirered
//  src/data/wild_encounters.json, FireRed).
const std::map<std::string, int> ROCK_TUNNEL_ENCOUNTER_RATES{
    {ROCK_TUNNEL_1F, 7},
    {ROCK_TUNNEL_B1F, 7},
    {ROUTE10, 21},
};

constexpr unsigned GAME_ODDS_SEEDS = 30;

//  08.01 whole, its wild battles at the game's own odds (OverworldSim::
//  rolls_encounter()), from each of these seeds: every run ends where it
//  should. Logs the runs' numbers and the step taps the game ignored.
UnitTestResult test_rock_tunnel_game_odds(Logger& logger){
    OverworldSim clean_sim = row_start_sim("08.01", SimScript{});
    if (const UnitTestResult clean = row_checked(clean_sim, "08.01", {ROCK_TUNNEL_TRAINERS}, MapPosition{ROUTE10, {8, 58}}); clean.result != UnitTestResult::PASSED){
        return clean;
    }
    std::vector<RowRunNumbers> runs;
    std::string failures;
    int ignored = 0;
    for (unsigned seed = 1; seed <= GAME_ODDS_SEEDS; seed++){
        SimScript script;
        script.encounter_rates = ROCK_TUNNEL_ENCOUNTER_RATES;
        script.encounter_seed = seed;
        script.dark_maps = ROCK_TUNNEL_DARK;
        OverworldSim sim = row_start_sim("08.01", script);
        const UnitTestResult result = row_checked(sim, "08.01", {ROCK_TUNNEL_TRAINERS}, MapPosition{ROUTE10, {8, 58}});
        const bool passed = result.result == UnitTestResult::PASSED;
        runs.push_back(row_numbers(sim.state(), passed, clean_sim.state()));
        ignored += sim.state().step_taps_ignored;
        if (!passed){
            failures += std::format("{}Seed {}: {}", failures.empty() ? "" : " | ", seed, result.message);
        }
    }
    logger.log(std::format(
        "08.01 at the game's encounter odds, {} step taps ignored: {}", ignored, numbers_summary(runs)
    ));
    if (!failures.empty()){
        return failures;
    }
    return true;
}


//  08.02 on the sim: no trainer met, the ledge hopped, Lavender's nurse.
UnitTestResult test_lavender_row(Logger&){
    return row_on_sim("08.02", SimScript{}, {{}}, MapPosition{LAVENDER_CENTER, {7, 4}});
}

//  Where an open leg of two tiles or more ends a tile short or long, the
//  next leg starts a tile off. From each such start (an open tile), the
//  rest of the row still ends where the table says. A leg to a wall or
//  onto a warp ends on its tile, and a one-tile leg is one step.
UnitTestResult tile_off_after_open_legs(std::string_view id){
    const StoryCheckpoint& row = checkpoint(id);
    const std::vector<MapData>& maps = story_maps();
    std::optional<RouteStep> before;
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    for (size_t s = 0; s < row.steps.size(); s++){
        const StoryStep& step = row.steps[s];
        const bool walks = step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS;
        if (!walks){
            //  A step that isn't a walk starts where the table says.
            before.reset();
            if (step.pose_after){
                at = *story_pose(Pokemon::pose_of(*step.pose_after));
            }else if (step.action == CUT_TREE_VARIANT){
                const Direction direction = Pokemon::story_arg<Direction>(step.args, "direction");
                at = StoryPose{at.map, at.tile.next(direction), direction};
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
    return tile_off_after_open_legs("08.01");
}

UnitTestResult test_lavender_tile_off(Logger&){
    return tile_off_after_open_legs("08.02");
}

//  08.02 saves in Lavender Town's Pokemon Center, LAVENDER TOWN on the
//  save card; 08.01 on Route 10.
UnitTestResult test_save_places(Logger&){
    if (save_card_place(LAVENDER_CENTER) != "lavender-town" || save_card_place(ROUTE10) != "route-10"){
        return "Wrong save card places for Lavender Town's Center or Route 10.";
    }
    return true;
}

//  08.03 on the sim: the Mart, Route 8 with only Rich fought (the walker
//  keeps out of the other trainers' lines), the Underground Path, the
//  Pidgey's grass and Celadon City, below its Center's door. The Center
//  has no map: its way in is stubbed.
UnitTestResult test_celadon_way_row(Logger&){
    return row_on_sim("08.03", SimScript{}, {{"old_man_1"}}, MapPosition{"CeladonCity", {48, 12}});
}

//  08.03's walk into Route 7's grass with a wild battle on its one grass
//  step (Route 7's land rate is 21): it still ends on the Pidgey's tile.
UnitTestResult test_route7_grass_battles(Logger&){
    const StoryCheckpoint& row = checkpoint("08.03");
    const size_t index = step_index(row, "pidgey_grass");
    const MapPosition end{"Route7", {14, 6}};
    SimScript script;
    script.encounters.push_back(SimEncounter{.grass_step = 1});
    StoryPose at{"Route7", {7, 15}, Direction::SOUTH};
    OverworldSim sim(maps_after_route10(), MapPosition{at.map, at.tile}, *at.facing, script);
    if (const std::string failure = row_step_on_sim(sim, at, row.steps[index], row.id); !failure.empty()){
        return failure;
    }
    if (!(sim.state().position == end) || sim.state().wild_battles != 1){
        return std::format("Ended on {} after {} wild battles, not on {} after 1.", to_string(sim.state().position), sim.state().wild_battles, to_string(end));
    }
    return true;
}

//  The maps after Gamer Rich's battle: Rich, beaten, stands where he
//  walked down to (55, 17).
std::vector<MapData> maps_after_rich(){
    std::vector<MapData> maps = maps_after_route10();
    for (MapData& map : maps){
        if (map.name != "Route8"){
            continue;
        }
        for (MapObject& object : map.objects){
            if (object.name == "old_man_1"){
                object.y = 17;
                object.trainer.reset();
            }
        }
    }
    return maps;
}

//  The walker believes the player stands on `believed`, a tile 08.03's
//  steps from Rich's battle on pass through, while the player stands on
//  `actual`.
struct OffStart{
    TilePos believed;
    TilePos actual;
};

const OffStart ROUTE8_OFF_STARTS[] = {
    {{55, 18}, {58, 18}},
    {{55, 18}, {52, 18}},
    {{48, 7}, {45, 7}},
    {{48, 7}, {50, 7}},
    {{26, 7}, {29, 7}},
    {{26, 7}, {26, 6}},
    {{26, 7}, {26, 9}},
    {{26, 7}, {28, 10}},
};

//  08.03's steps from `first` to `last`, as the table lays them out from
//  `at`: the first that starts on `tile`, and where the table says the
//  player stands then. Nothing if none does.
std::optional<std::pair<size_t, StoryPose>> step_starting_on(
    const StoryCheckpoint& row, size_t first, size_t last, StoryPose at, const TilePos& tile
){
    for (size_t c = first; c <= last; c++){
        if (at.map == "Route8" && at.tile == tile){
            return std::make_pair(c, at);
        }
        const std::optional<std::vector<RouteStep>> route = walked_route(at, row.steps[c]);
        if (!route || route->empty()){
            return std::nullopt;
        }
        at = StoryPose{route->back().end.map, route->back().end.point, route->back().facing_after};
    }
    return std::nullopt;
}

//  Console B, 2026-11-09 (08.03, three runs): after Route 8's open legs
//  the player stood 3 tiles east of where the walker believed, the hold
//  north to the Underground Path's door ran up column 17 to the cliff
//  (16, 2), and the step onto the door bumped the house: "no fade in and
//  out after walking onto the warp". From a start up to 3 tiles off, the
//  steps on to Route 7 meet no trainer and come out of the Underground
//  Path where the table says.
UnitTestResult test_route8_west_off_start(Logger&){
    const StoryCheckpoint& row = checkpoint("08.03");
    const size_t first = step_index(row, "west_of_glenn_column");
    const size_t last = step_index(row, "route7_underground_exit");
    const StoryPose after_rich{"Route8", {55, 18}, Direction::NORTH};
    const std::vector<MapData> maps = maps_after_rich();
    const MapPosition end{"Route7", {7, 15}};
    std::string failures;
    for (const OffStart& off : ROUTE8_OFF_STARTS){
        const std::optional<std::pair<size_t, StoryPose>> from = step_starting_on(row, first, last, after_rich, off.believed);
        std::string failure = from ? "" : "no step starts there.";
        StoryPose at = from ? from->second : after_rich;
        OverworldSim sim(maps, MapPosition{at.map, off.actual}, at.facing.value_or(Direction::NORTH));
        for (size_t c = from ? from->first : last + 1; c <= last && failure.empty(); c++){
            failure = row_step_on_sim(sim, at, row.steps[c], row.id);
        }
        if (failure.empty() && !(sim.state().position == end)){
            failure = std::format("ended on {}, not {}.", to_string(sim.state().position), to_string(end));
        }
        if (failure.empty() && !sim.state().trainers_beaten.empty()){
            failure = "fought " + sim.state().trainers_beaten.front() + ".";
        }
        if (failure.empty() && !sim.state().violations.empty()){
            failure = "a violation on the way: " + sim.state().violations.back();
        }
        if (!failure.empty()){
            failures += std::format("{}from {}, not {}: {}", failures.empty() ? "" : " | ", to_string(off.actual), to_string(off.believed), failure);
        }
    }
    if (!failures.empty()){
        return failures;
    }
    return true;
}

//  08.03 saves in Celadon City's Pokemon Center: CELADON CITY on the save
//  card.
UnitTestResult test_celadon_save_place(Logger&){
    if (save_card_place("CeladonCity_PokemonCenter_1F") != "celadon-city"){
        return "Wrong save card place for Celadon City's Pokemon Center.";
    }
    return true;
}

//  09.01 on the sim: out of Celadon's Center, the Game Corner's poster
//  Grunt, the switch, the Rocket Hideout down to B4F's Lift Key Grunt, the
//  LIFT KEY, and back up to B3F below its Rare Candy.
//  A battle a talk starts (the poster Grunt's) is the sim's to skip: it
//  leaves the player where the plan says, and the trainer walks off
//  (talk_battle_hide_flag()).
UnitTestResult test_hideout_row(Logger&){
    const StoryCheckpoint& row = checkpoint("09.01");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::vector<MapData> maps = maps_after_route10();
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
    std::vector<std::string> fought;
    for (const StoryStep& step : row.steps){
        const std::optional<std::string> hidden = talk_battle_hide_flag(step);
        if (!hidden){
            if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
                return "09.01: ended on " + to_string(sim->state().position) + "; " + failure;
            }
            continue;
        }
        at = *story_pose(Pokemon::pose_of(*step.pose_after));
        for (MapData& map : maps){
            hide_objects(map, {*hidden});
        }
        fought = sim->state().trainers_beaten;
        sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing);
    }
    fought.insert(fought.end(), sim->state().trainers_beaten.begin(), sim->state().trainers_beaten.end());
    if (fought != std::vector<std::string>{"rocket_m"}){
        return std::format("09.01: fought {} trainers on the sim, not the Lift Key Grunt alone.", fought.size());
    }
    if (!sim->state().violations.empty()){
        return "09.01: a violation on the way: " + sim->state().violations.back();
    }
    const MapPosition end{"RocketHideout_B3F", {12, 13}};
    if (!(sim->state().position == end)){
        return "09.01: ended on " + to_string(sim->state().position) + ", not " + to_string(end) + ".";
    }
    return true;
}

} // namespace


//  09.02 on the sim: from below B3F's Rare Candy back up to B2F, through
//  its arrows (the MOON STONE ball stands in the way and is picked up),
//  the lift to B4F, the two door guards, Giovanni, and beside the SILPH
//  SCOPE. The talk-started battles are the sim's to skip; the MOON
//  STONE's pickup hides its ball, as the game does.
UnitTestResult test_hideout_giovanni_row(Logger&){
    const StoryCheckpoint& row = checkpoint("09.02");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::vector<MapData> maps = maps_after_route10();
    if (std::none_of(maps.begin(), maps.end(), [](const MapData& map){ return map.name == LIFT_MAP; })){
        maps.push_back(rocket_hideout_lift_room());
    }
    SimScript script;
    script.lift_floors[std::string(LIFT_MAP)] = rocket_hideout_lift_exits();
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing, script);
    for (const StoryStep& step : row.steps){
        if (step.name == "escape_rope_hideout"){
            break;
        }
        if (step.kind == StoryStepKind::BATTLE){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            continue;
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return "09.02: ended on " + to_string(sim->state().position) + "; " + failure;
        }
        if (step.name == "moon_stone"){
            for (MapData& map : maps){
                hide_objects(map, {"FLAG_HIDE_ROCKET_HIDEOUT_B2F_MOON_STONE"});
            }
            sim = std::make_unique<OverworldSim>(maps, sim->state().position, sim->state().facing, script);
        }
    }
    if (!sim->state().trainers_beaten.empty()){
        return std::format("09.02: walked into {} trainers' lines on the sim.", sim->state().trainers_beaten.size());
    }
    if (!sim->state().violations.empty()){
        return "09.02: a violation on the way: " + sim->state().violations.back();
    }
    const MapPosition end{"RocketHideout_B4F", {21, 5}};
    if (!(sim->state().position == end)){
        return "09.02: ended on " + to_string(sim->state().position) + ", not " + to_string(end) + ".";
    }
    return true;
}

//  09.02's walk past B2F's Grunt (20, 6), who looks around (sight 2): no
//  tile of a walk leg, nor the tile past an open leg's end, is in a way he
//  may look. On console B, 2026-11-11 15:03, a south 2 to (19, 5) whose
//  first tap read as a turn went on to (19, 6), in his west line, and the
//  next leg met him.
UnitTestResult test_b2f_grunt_lines_with_tile_off(Logger&){
    const StoryCheckpoint& row = checkpoint("09.02");
    const auto step = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& s){ return s.name == "b2f_arrows_to_moon_stone"; });
    const std::vector<MapData>& maps = story_maps();
    const auto map = std::find_if(maps.begin(), maps.end(), [](const MapData& data){ return data.name == "RocketHideout_B2F"; });
    if (step == row.steps.end() || map == maps.end()){
        return "09.02: no b2f_arrows_to_moon_stone or no RocketHideout_B2F.";
    }
    const StoryPose at{"RocketHideout_B2F", {21, 2}, Direction::EAST};
    const std::optional<std::vector<RouteStep>> route = walked_route(at, *step);
    if (!route){
        return "09.02: b2f_arrows_to_moon_stone has no route.";
    }
    TilePos from = at.tile;
    for (const RouteStep& leg : *route){
        if (leg.action != RouteAction::WALK || leg.spin_tiles){
            break;
        }
        std::vector<TilePos> exposed;
        TilePos tile = from;
        for (int c = 0; c < leg.tiles; c++){
            tile = tile.next(leg.direction);
            exposed.push_back(tile);
        }
        const TilePos past = tile.next(leg.direction);
        if (!leg.to_wall && in_bounds(*map, past) && tile_at(*map, past) == TileKind::OPEN){
            exposed.push_back(past);
        }
        for (const TilePos& seen : exposed){
            for (const MapObject& object : map->objects){
                if (trainer_may_see(*map, object, seen)){
                    return std::format("09.02: leg {} may end on {}, in the sight of the trainer on ({}, {}).", to_string(leg), to_string(seen), object.x, object.y);
                }
            }
        }
        from = leg.end.point;
    }
    return true;
}

//  The tiles `route` walks from `from`, each on its map: a walk leg's
//  tiles one by one, a slide's stop alone (the slide is the arrows'), a
//  warp's tiles up to it and the tile it lands on.
std::vector<MapPosition> walked_tiles(MapPosition from, const std::vector<RouteStep>& route){
    std::vector<MapPosition> tiles;
    for (const RouteStep& leg : route){
        if (leg.action != RouteAction::WALK){
            continue;
        }
        const int straight = leg.spin_tiles ? 0 : leg.warps ? leg.tiles - 1 : leg.tiles;
        TilePos tile = from.point;
        for (int c = 0; c < straight; c++){
            tile = tile.next(leg.direction);
            tiles.push_back({from.map, tile});
        }
        if (straight != leg.tiles){
            tiles.push_back(leg.end);
        }
        from = leg.end;
    }
    return tiles;
}

//  09.02 goes the direct way on B3F and B2F: no tile there is walked twice.
//  The old B2F legs went up column 17 to the wall (17, 5) and back down a
//  tile to (17, 6). The one forced turn back, east from the stop (1, 4) to
//  (2, 4), which the slide passed, is not counted: a slide counts its stop
//  alone. The MOON STONE stays: its ball (2, 5) is an object, which stops
//  the player (pokefirered DoForcedMovement()), in the one way down from
//  row 4. below_lift's two tiles to the wall (30, 17) and back stay too:
//  console B, 2026-11-11 01:04, its taps ran on past the lift to the wall
//  (test_below_lift_one_backtrack()).
UnitTestResult test_hideout_giovanni_row_no_tile_twice(Logger&){
    const StoryCheckpoint& row = checkpoint("09.02");
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    std::vector<MapData> maps = maps_after_route10();
    if (std::none_of(maps.begin(), maps.end(), [](const MapData& map){ return map.name == LIFT_MAP; })){
        maps.push_back(rocket_hideout_lift_room());
    }
    SimScript script;
    script.lift_floors[std::string(LIFT_MAP)] = rocket_hideout_lift_exits();
    auto sim = std::make_unique<OverworldSim>(maps, MapPosition{at.map, at.tile}, *at.facing, script);
    std::vector<MapPosition> walked{{at.map, at.tile}};
    for (const StoryStep& step : row.steps){
        if (step.name == "escape_rope_hideout"){
            break;
        }
        if (step.kind == StoryStepKind::BATTLE){
            at = *story_pose(Pokemon::pose_of(*step.pose_after));
            continue;
        }
        if (step.kind == StoryStepKind::ROUTE || step.kind == StoryStepKind::LEGS){
            const std::optional<std::vector<RouteStep>> route = walked_route(at, step);
            if (!route){
                return std::format("09.02/{}: no route.", step.name);
            }
            for (const MapPosition& tile : walked_tiles({at.map, at.tile}, *route)){
                const bool floor = tile.map == "RocketHideout_B3F" || tile.map == "RocketHideout_B2F";
                if (!floor || step.name == "below_lift"){
                    continue;
                }
                if (std::find(walked.begin(), walked.end(), tile) != walked.end()){
                    return std::format("09.02/{}: walks {} a second time.", step.name, to_string(tile));
                }
                walked.push_back(tile);
            }
        }
        if (const std::string failure = row_step_on_sim(*sim, at, step, row.id); !failure.empty()){
            return std::format("09.02: {}", failure);
        }
        if (step.name == "moon_stone"){
            for (MapData& map : maps){
                hide_objects(map, {"FLAG_HIDE_ROCKET_HIDEOUT_B2F_MOON_STONE"});
            }
            sim = std::make_unique<OverworldSim>(maps, sim->state().position, sim->state().facing, script);
        }
    }
    return true;
}

const AutoStoryLavenderCase AUTO_STORY_LAVENDER_CASES[] = {
    {.input = {"08.01 the tree, Rock Tunnel's nine trainers, out of its south mouth", test_rock_tunnel_row}, .expectation = {}},
    {.input = {"08.01 a wild battle mid-leg in the tunnel", test_rock_tunnel_wild_battles}, .expectation = {}},
    {.input = {"08.01 dark: each trainer's battle ends on the dark cave's overworld", test_rock_tunnel_dark_battle_ends}, .expectation = {}},
    {.input = {"08.01 a tile off after each open leg", test_rock_tunnel_tile_off}, .expectation = {}},
    {.input = {"09.02 B2F's turning Grunt: no leg ends in his lines, a tile off included", test_b2f_grunt_lines_with_tile_off}, .expectation = {}},
    {.input = {"08.01 B1F row 37 with a wild battle every 10 to 14 steps", test_b1f_row37_battles_every_12}, .expectation = {}},
    {.input = {"08.01 a wild battle every 7 to 25 steps, 12 seeds", test_rock_tunnel_frequent_battles}, .expectation = {}},
    {.input = {"08.01 wild battles at the game's odds, 30 seeds", test_rock_tunnel_game_odds}, .expectation = {}},
    {.input = {"08.01 B1F a wild battle into row 38, then a step short: Martha still sees the player", test_b1f_martha_after_row38_battle}, .expectation = {}},
    {.input = {"08.01 B1F dark, the 13:35 legs: a battle at the end of the walk back along row 38", test_b1f_dark_row38_battle_old_legs}, .expectation = {}},
    {.input = {"08.01 B1F dark, a wild battle on any step to Martha's line", test_b1f_dark_martha_battle_anywhere}, .expectation = {}},
    {.input = {"08.01 B1F dark, winston_line: a wild battle on the 2nd tap, the 3rd after it", test_winston_line_battle_on_tap_2}, .expectation = {}},
    {.input = {"08.02 Route 10 south, the ledge, Lavender Town's Pokemon Center", test_lavender_row}, .expectation = {}},
    {.input = {"08.02 a tile off after each open leg", test_lavender_tile_off}, .expectation = {}},
    {.input = {"08.01 and 08.02 save on Route 10 and in Lavender Town", test_save_places}, .expectation = {}},
    {.input = {"08.03 Lavender's Mart, Route 8 past Rich alone, the Underground Path, Route 7 to Celadon City", test_celadon_way_row}, .expectation = {}},
    {.input = {"08.03 a wild battle on the step into Route 7's grass", test_route7_grass_battles}, .expectation = {}},
    {.input = {"08.03 Route 8 west from a start up to 3 tiles off", test_route8_west_off_start}, .expectation = {}},
    {.input = {"08.03 saves in Celadon City's Pokemon Center", test_celadon_save_place}, .expectation = {}},
    {.input = {"09.01 the Game Corner's poster, the Rocket Hideout to B4F's LIFT KEY and B3F's Rare Candy", test_hideout_row}, .expectation = {}},
    {.input = {"09.02 B2F's arrows, the lift, the door guards, Giovanni and the SILPH SCOPE", test_hideout_giovanni_row}, .expectation = {}},
    {.input = {"09.02 no tile of B3F or B2F walked twice: no way out and back", test_hideout_giovanni_row_no_tile_twice}, .expectation = {}},
};

void add_tests_AutoStoryLavender(UnitTestDatabase& database){
    for (const auto& item : AUTO_STORY_LAVENDER_CASES){
        database.add<Test_AutoStoryLavender>(item);
    }
    add_tests_AutoStoryResources(database);
    add_tests_AutoStoryFaultSweep(database);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
