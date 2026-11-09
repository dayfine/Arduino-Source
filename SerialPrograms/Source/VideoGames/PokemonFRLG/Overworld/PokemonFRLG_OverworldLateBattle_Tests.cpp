/*  Overworld Late Battle Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A wild battle seen only after an open leg's hold, on the sim: the walker
 *  must place the player within a few tiles of where the battle stopped
 *  them, find them again and finish the route.
 *
 */

#include <cstdio>
#include <format>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldLateBattle.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using namespace std::chrono_literals;

struct LateBattleInput{
    std::string name;
    //  The leg's step (from 1) the battle starts on.
    int battle_step = 1;
};

struct LateBattleExpectation{
    //  The most the walker's bound on the player's tile may span, in tiles.
    int max_spread = 0;
};

struct LateBattleCase{
    LateBattleInput input;
    LateBattleExpectation expectation;
};

const std::vector<MapData>& maps(){
    static const std::vector<MapData> MAPS = overworld_maps();
    return MAPS;
}

//  The bound run_route() logged for the stop: "<leg> stopped <min> to <max>
//  tiles in; ...".
std::optional<WalkedTiles> logged_bound(const std::vector<std::string>& log){
    for (const std::string& line : log){
        const size_t at = line.find(" stopped ");
        WalkedTiles bound;
        if (at != std::string::npos && std::sscanf(line.c_str() + at, " stopped %d to %d tiles in", &bound.min, &bound.max) == 2){
            return bound;
        }
    }
    return std::nullopt;
}

std::string describe(const std::string& failure, const SimState& state, const std::vector<std::string>& log){
    std::string out = std::format("\n  ended on {}, failure: \"{}\"", to_string(state.position), failure);
    for (const std::string& line : log){
        out += "\n  " + line;
    }
    for (const std::string& line : state.log){
        out += "\n  " + line;
    }
    return out;
}

//  Console B, FRLG 00.27 run 2, 2026-11-09 (~/FRLGLab/captures/
//  2026-11-09-0027-route10-legs.log): on the open leg "east 11" across
//  Route 10, wild battles (Voltorb) showed only after the hold. The walker
//  took the player to be anywhere on the leg ("stopped 0 to 11 tiles in")
//  and walked "east 12 (to wall)" to find them, into a second battle. Here
//  the leg crosses Route 10's grass on row 10, the battle's flashes are
//  missed, its text box ("Wild <Pokemon> appeared!") shows
//  LATE_WILD_BATTLE_DELAY after its step and its HP boxes 800 ms after
//  that, as in Mt. Moon (console B, 2026-11-03, row 00.13). After running,
//  the walker must bound the player to a few tiles around the battle's.
class Test_OverworldLateBattle : public UnitTest{
public:
    explicit Test_OverworldLateBattle(LateBattleCase test_case)
        : UnitTest("PokemonFRLG::OverworldLateBattle - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        try{
            return run_case();
        }catch (Exception& e){
            return std::string(e.name()) + ": " + e.message();
        }
    }

private:
    UnitTestResult run_case() const{
        const MapPosition start{"Route10", {5, 10}};
        const MapPosition goal{"Route10", {16, 10}};
        RouteStep east;
        east.direction = Direction::EAST;
        east.tiles = 11;
        east.wild = true;
        east.facing_after = Direction::EAST;
        east.end = goal;
        SimScript script;
        script.encounters = {{.grass_step = m_case.input.battle_step, .seen_late = true}};
        script.timing.battle_opening = OverworldWalkSteps::LATE_WILD_BATTLE_DELAY;
        script.timing.battle_screen = OverworldWalkSteps::LATE_WILD_BATTLE_DELAY + 800ms;
        OverworldSim sim(maps(), start, Direction::EAST, script);
        OverworldSimWalker walker(sim);
        const RoutePlanning planning{.maps = &maps(), .costs = default_route_costs(), .start = start, .label = "Sim"};
        const std::string failure = walker.run({east}, planning);
        const std::string seen = describe(failure, sim.state(), walker.log());
        if (!failure.empty() || !(sim.state().position == goal) || !sim.state().violations.empty()){
            return "Expected the battle run from and " + to_string(goal) + " reached:" + seen;
        }
        const std::optional<WalkedTiles> bound = logged_bound(walker.log());
        const int battle = m_case.input.battle_step;
        if (!bound || bound->min > battle || bound->max < battle || bound->max - bound->min > m_case.expectation.max_spread){
            return std::format(
                "Expected the player placed within {} tiles around tile {} of the leg, got {}:{}",
                m_case.expectation.max_spread, battle,
                bound ? std::format("{} to {}", bound->min, bound->max) : std::string("no bound"), seen
            );
        }
        return true;
    }

    LateBattleCase m_case;
};

} // namespace


const LateBattleCase LATE_BATTLE_CASES[] = {
    {
        .input = {.name = "a battle on step 3 of an open leg, seen after the hold", .battle_step = 3},
        .expectation = {.max_spread = 2},
    },
    {
        .input = {.name = "a battle on step 6 of an open leg, seen after the hold", .battle_step = 6},
        .expectation = {.max_spread = 2},
    },
    {
        .input = {.name = "a battle on step 10, its HP boxes after the leg's watch", .battle_step = 10},
        .expectation = {.max_spread = 2},
    },
};

void add_tests_OverworldLateBattle(UnitTestDatabase& database){
    for (const LateBattleCase& item : LATE_BATTLE_CASES){
        database.add<Test_OverworldLateBattle>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
