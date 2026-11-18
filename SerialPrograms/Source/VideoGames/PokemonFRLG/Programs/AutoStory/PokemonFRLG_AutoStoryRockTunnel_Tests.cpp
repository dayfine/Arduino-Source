/*  AutoStory Rock Tunnel Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  08.01 through Rock Tunnel on the overworld sim, one floor at a time:
 *  each floor from where a clean run of the row reaches it, with wild
 *  battles every few steps and at the game's own odds. One case a floor
 *  fits STAGE_BUDGET, where one on the whole row ran past it.
 *
 */

#include <algorithm>
#include <format>
#include <map>
#include <numeric>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMaps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldObjects.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRouteSteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryRowSim.h"
#include "PokemonFRLG_AutoStoryRockTunnel_Tests.h"
#include "PokemonFRLG_AutoStorySimBudget.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

const std::string ROCK_TUNNEL_1F = "RockTunnel_1F";
const std::string ROCK_TUNNEL_B1F = "RockTunnel_B1F";
const std::string ROUTE10 = "Route10";

//  Rock Tunnel has no Flash: both floors are dark.
const std::set<std::string> ROCK_TUNNEL_DARK{ROCK_TUNNEL_1F, ROCK_TUNNEL_B1F};

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

//  Rock Tunnel's floors as 08.01 walks them, in order: its steps on Route 10,
//  then each visit to 1F or B1F, which ends where the next starts. A test
//  on a floor fits STAGE_BUDGET where one on the whole row ran past it
//  (Fork Draft CI, 2026-11-16: 30 seeds at the game's odds took 314 s).
const std::string_view ROCK_TUNNEL_FLOOR_NAMES[] = {
    "Route 10 to the tunnel",
    "1F, Ashton, down the south ladder",
    "B1F, Winston, Martha and Allen",
    "1F, Lenny",
    "B1F, Sofia and Dudley",
    "1F, Dana, out of the south mouth",
};

//  One floor of 08.01: steps [first, end) of the row, from where a clean
//  run of the whole row stood at `first`, to where it stood at `end`.
struct RockTunnelFloor{
    size_t first = 0;
    size_t end = 0;
    StoryPose at = {};
    MapPosition start = {};
    Direction facing = Direction::SOUTH;
    //  The trainers fought before it (trainer_id()), on any floor.
    std::set<std::string> beaten = {};
    //  The trainers it fights, by name, in order.
    std::vector<std::string> fought = {};
    MapPosition finish = {};
};

//  The trainers of `sim`'s current map that are no longer trainers, as
//  trainer_id() names them on `maps`, the maps the sim started on.
std::set<std::string> trainers_gone(const OverworldSim& sim, const std::vector<MapData>& maps){
    std::set<std::string> gone;
    const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& data){ return data.name == sim.state().position.map; });
    if (map == maps.end()){
        return gone;
    }
    const std::vector<MapObject>& now = sim.objects();
    for (size_t c = 0; c < map->objects.size() && c < now.size(); c++){
        if (map->objects[c].trainer && !now[c].trainer){
            gone.insert(trainer_id(*map, map->objects[c]));
        }
    }
    return gone;
}

//  08.01's floors, from one clean run of the row.
std::vector<RockTunnelFloor> find_rock_tunnel_floors(){
    const StoryCheckpoint& row = checkpoint("08.01");
    const std::vector<MapData> maps = maps_after_route10();
    StoryPose at = *story_pose(Pokemon::pose_of(*row.start));
    OverworldSim sim(maps, MapPosition{at.map, at.tile}, *at.facing, SimScript{});
    std::vector<RockTunnelFloor> floors;
    std::set<std::string> beaten;
    size_t fought_before = 0;
    const auto close_floor = [&](size_t end){
        RockTunnelFloor& floor = floors.back();
        floor.end = end;
        floor.finish = sim.state().position;
        const std::vector<std::string>& all = sim.state().trainers_beaten;
        floor.fought.assign(all.begin() + (std::ptrdiff_t)fought_before, all.end());
        fought_before = all.size();
    };
    for (size_t c = 0; c < row.steps.size(); c++){
        const std::string& map = sim.state().position.map;
        if (floors.empty() || (map != floors.back().start.map && ROCK_TUNNEL_DARK.contains(map))){
            if (!floors.empty()){
                close_floor(c);
            }
            floors.push_back({.first = c, .at = at, .start = sim.state().position, .facing = sim.state().facing, .beaten = beaten});
        }
        if (const std::string failure = row_step_on_sim(sim, at, row.steps[c], row.id); !failure.empty()){
            throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format("08.01's clean run, finding its floors: {}", failure));
        }
        beaten.merge(trainers_gone(sim, maps));
    }
    close_floor(row.steps.size());
    return floors;
}

//  find_rock_tunnel_floors(), run once for all the floor tests.
const std::vector<RockTunnelFloor>& rock_tunnel_floors(){
    static const std::vector<RockTunnelFloor> FLOORS = find_rock_tunnel_floors();
    return FLOORS;
}

//  The sim at `floor`'s start, its earlier trainers beaten, with `script`.
OverworldSim floor_start_sim(const RockTunnelFloor& floor, SimScript script){
    return OverworldSim(with_trainers_beaten(maps_after_route10(), floor.beaten), floor.start, floor.facing, std::move(script));
}

//  `floor`'s steps on `sim`, from its start: the failure, or empty if it
//  ended where the clean run did, with no violation on the way, having
//  fought the same trainers.
std::string floor_checked(const RockTunnelFloor& floor, OverworldSim& sim){
    const StoryCheckpoint& row = checkpoint("08.01");
    StoryPose at = floor.at;
    for (size_t c = floor.first; c < floor.end; c++){
        if (const std::string failure = row_step_on_sim(sim, at, row.steps[c], row.id); !failure.empty()){
            return std::format("ended on {}; {}", to_string(sim.state().position), failure);
        }
    }
    if (sim.state().trainers_beaten != floor.fought){
        std::string fought;
        for (const std::string& name : sim.state().trainers_beaten){
            fought += std::format(" {}", name);
        }
        return std::format("fought{}.", fought.empty() ? " no one" : fought);
    }
    if (!sim.state().violations.empty()){
        return std::format("a violation on the way: {}", sim.state().violations.back());
    }
    if (!(sim.state().position == floor.finish)){
        return std::format("ended on {}, not {}.", to_string(sim.state().position), to_string(floor.finish));
    }
    return "";
}

//  Floor `index` of rock_tunnel_floors(), checked to be the one
//  ROCK_TUNNEL_FLOOR_NAMES names by count.
const RockTunnelFloor& rock_tunnel_floor(size_t index){
    const std::vector<RockTunnelFloor>& floors = rock_tunnel_floors();
    if (floors.size() != std::size(ROCK_TUNNEL_FLOOR_NAMES)){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, std::format(
            "08.01 walks {} floors, but ROCK_TUNNEL_FLOOR_NAMES names {}.", floors.size(), std::size(ROCK_TUNNEL_FLOOR_NAMES)
        ));
    }
    return floors[index];
}

//  A floor of 08.01 with a wild battle every 7 to 25 steps on the tunnel's
//  floor, the gaps drawn from each of these seeds: it ends where it should.
UnitTestResult test_floor_frequent_battles(size_t index){
    const RockTunnelFloor& floor = rock_tunnel_floor(index);
    std::string failures;
    for (unsigned seed = 1; seed <= 12; seed++){
        std::mt19937 random(seed);
        std::uniform_int_distribution<int> gap(7, 25);
        std::vector<int> gaps;
        for (int c = 0; c < 400; c++){
            gaps.push_back(gap(random));
        }
        OverworldSim sim = floor_start_sim(floor, battles_every(gaps));
        if (const std::string failure = floor_checked(floor, sim); !failure.empty()){
            failures += std::format("{}Seed {}: {}", failures.empty() ? "" : " | ", seed, failure);
        }
    }
    if (!failures.empty()){
        return failures;
    }
    return true;
}

//  A floor of 08.01, its wild battles at the game's own odds (OverworldSim::
//  rolls_encounter()), from each of these seeds: every run ends where it
//  should. Logs the runs' numbers and the step taps the game ignored.
UnitTestResult test_floor_game_odds(Logger& logger, size_t index){
    const RockTunnelFloor& floor = rock_tunnel_floor(index);
    OverworldSim clean_sim = floor_start_sim(floor, SimScript{});
    if (const std::string clean = floor_checked(floor, clean_sim); !clean.empty()){
        return std::format("With no wild battle: {}", clean);
    }
    std::vector<RowRunNumbers> runs;
    std::string failures;
    int ignored = 0;
    for (unsigned seed = 1; seed <= GAME_ODDS_SEEDS; seed++){
        SimScript script;
        script.encounter_rates = ROCK_TUNNEL_ENCOUNTER_RATES;
        script.encounter_seed = seed;
        script.dark_maps = ROCK_TUNNEL_DARK;
        OverworldSim sim = floor_start_sim(floor, script);
        const std::string failure = floor_checked(floor, sim);
        runs.push_back(row_numbers(sim.state(), failure.empty(), clean_sim.state()));
        ignored += sim.state().step_taps_ignored;
        if (!failure.empty()){
            failures += std::format("{}Seed {}: {}", failures.empty() ? "" : " | ", seed, failure);
        }
    }
    logger.log(std::format(
        "08.01 {} at the game's encounter odds, {} step taps ignored: {}",
        ROCK_TUNNEL_FLOOR_NAMES[index], ignored, numbers_summary(runs)
    ));
    if (!failures.empty()){
        return failures;
    }
    return true;
}

struct RockTunnelFloorInput{
    size_t floor = 0;
    //  The floor at the game's odds (test_floor_game_odds()), or with a
    //  battle every few steps (test_floor_frequent_battles()).
    bool game_odds = false;
};

struct RockTunnelFloorExpectation{
    std::string description;
};

struct RockTunnelFloorCase{
    std::string name;
    RockTunnelFloorInput input;
    RockTunnelFloorExpectation expectation;
};

class Test_AutoStoryRockTunnel : public UnitTest{
public:
    explicit Test_AutoStoryRockTunnel(RockTunnelFloorCase test_case)
        : UnitTest(std::format("PokemonFRLG::AutoStory - Simulation: {}", test_case.name))
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger& logger, CancellableScope&) const override{
        try{
            return timed_case(logger, m_case.name, [&]{
                return m_case.input.game_odds
                    ? test_floor_game_odds(logger, m_case.input.floor)
                    : test_floor_frequent_battles(m_case.input.floor);
            });
        }catch (Exception& e){
            return std::format("{}: {}", e.name(), e.message());
        }
    }

private:
    RockTunnelFloorCase m_case;
};

//  Each floor with a battle every few steps and at the game's odds
//  (GAME_ODDS_SEEDS), floor by floor in ROCK_TUNNEL_FLOOR_NAMES' order.
const RockTunnelFloorCase ROCK_TUNNEL_FLOOR_CASES[] = {
    {.name = "08.01 Route 10 to the tunnel: a wild battle every 7 to 25 steps, 12 seeds", .input = {.floor = 0, .game_odds = false}, .expectation = {.description = "every seed ends the floor where a clean run does"}},
    {.name = "08.01 Route 10 to the tunnel: wild battles at the game's odds, 30 seeds", .input = {.floor = 0, .game_odds = true}, .expectation = {.description = "every seed ends the floor where a clean run does; logs the numbers"}},
    {.name = "08.01 1F, Ashton, down the south ladder: a wild battle every 7 to 25 steps, 12 seeds", .input = {.floor = 1, .game_odds = false}, .expectation = {.description = "every seed ends the floor where a clean run does"}},
    {.name = "08.01 1F, Ashton, down the south ladder: wild battles at the game's odds, 30 seeds", .input = {.floor = 1, .game_odds = true}, .expectation = {.description = "every seed ends the floor where a clean run does; logs the numbers"}},
    {.name = "08.01 B1F, Winston, Martha and Allen: a wild battle every 7 to 25 steps, 12 seeds", .input = {.floor = 2, .game_odds = false}, .expectation = {.description = "every seed ends the floor where a clean run does"}},
    {.name = "08.01 B1F, Winston, Martha and Allen: wild battles at the game's odds, 30 seeds", .input = {.floor = 2, .game_odds = true}, .expectation = {.description = "every seed ends the floor where a clean run does; logs the numbers"}},
    {.name = "08.01 1F, Lenny: a wild battle every 7 to 25 steps, 12 seeds", .input = {.floor = 3, .game_odds = false}, .expectation = {.description = "every seed ends the floor where a clean run does"}},
    {.name = "08.01 1F, Lenny: wild battles at the game's odds, 30 seeds", .input = {.floor = 3, .game_odds = true}, .expectation = {.description = "every seed ends the floor where a clean run does; logs the numbers"}},
    {.name = "08.01 B1F, Sofia and Dudley: a wild battle every 7 to 25 steps, 12 seeds", .input = {.floor = 4, .game_odds = false}, .expectation = {.description = "every seed ends the floor where a clean run does"}},
    {.name = "08.01 B1F, Sofia and Dudley: wild battles at the game's odds, 30 seeds", .input = {.floor = 4, .game_odds = true}, .expectation = {.description = "every seed ends the floor where a clean run does; logs the numbers"}},
    {.name = "08.01 1F, Dana, out of the south mouth: a wild battle every 7 to 25 steps, 12 seeds", .input = {.floor = 5, .game_odds = false}, .expectation = {.description = "every seed ends the floor where a clean run does"}},
    {.name = "08.01 1F, Dana, out of the south mouth: wild battles at the game's odds, 30 seeds", .input = {.floor = 5, .game_odds = true}, .expectation = {.description = "every seed ends the floor where a clean run does; logs the numbers"}},
};

} // namespace


void add_tests_AutoStoryRockTunnel(UnitTestDatabase& database){
    for (const RockTunnelFloorCase& item : ROCK_TUNNEL_FLOOR_CASES){
        database.add<Test_AutoStoryRockTunnel>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
