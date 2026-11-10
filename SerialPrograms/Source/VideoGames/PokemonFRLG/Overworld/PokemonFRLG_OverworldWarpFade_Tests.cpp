/*  Overworld Warp Fade Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A warp whose fade is over before the hold onto it ends, on the sim: the
 *  walker must watch for the fade from the start of the hold.
 *
 */

#include <format>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

using namespace std::chrono_literals;

struct WarpFadeInput{
    std::string name;
    //  How long the fade shows, from the end of the step onto the warp.
    Milliseconds fade{0};
    //  SimWalkerRules::watch_fade_along_hold.
    bool watch_along_hold = true;
    //  SimScript::black_without_warp and SimWalkerRules::confirm_warp_destination.
    std::set<int> black_without_warp = {};
    bool confirm_destination = true;
};

struct WarpFadeExpectation{
    //  The failure the walk stops on; empty: the route is walked.
    std::string failure;
    int warps = 1;
};

struct WarpFadeCase{
    WarpFadeInput input;
    WarpFadeExpectation expectation;
};

const std::vector<MapData>& maps(){
    static const std::vector<MapData> MAPS = overworld_maps();
    return MAPS;
}

const MapPosition MART{"ViridianCity_Mart", {4, 3}};
//  Where the Mart's exit mat, (4, 7), puts the player.
const MapPosition OUTSIDE{"ViridianCity", {36, 20}};

//  00.27 (console B, 2026-11-09 01:41), the warp to CeruleanCity: "no fade
//  in and out after walking onto the warp", the watch for the fade having
//  started about 1.3 s into the hold, after the fade was over. Here the
//  walk out of Viridian Mart, south 5 onto its mat, is held as to a wall,
//  two tiles past the mat; a 200 ms fade is over before the hold ends.
class Test_OverworldWarpFade : public UnitTest{
public:
    explicit Test_OverworldWarpFade(WarpFadeCase test_case)
        : UnitTest("PokemonFRLG::OverworldWarpFade - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        RouteStep out;
        out.direction = Direction::SOUTH;
        out.tiles = 5;
        out.warps = true;
        out.to_wall = true;
        out.facing_after = Direction::SOUTH;
        out.end = OUTSIDE;
        SimScript script;
        script.timing.fade = m_case.input.fade;
        script.black_without_warp = m_case.input.black_without_warp;
        SimWalkerRules rules;
        rules.watch_fade_along_hold = m_case.input.watch_along_hold;
        rules.confirm_warp_destination = m_case.input.confirm_destination;
        OverworldSim sim(maps(), MART, Direction::SOUTH, script);
        OverworldSimWalker walker(sim, rules);
        const RoutePlanning planning{.maps = &maps(), .costs = default_route_costs(), .start = MART, .label = "Sim"};
        const std::string failure = walker.run({out}, planning);
        const std::string& expected = m_case.expectation.failure;
        const bool as_expected = expected.empty() ? failure.empty() : failure.find(expected) != std::string::npos;
        if (as_expected && sim.state().warps == m_case.expectation.warps){
            return true;
        }
        std::string log;
        for (const std::string& line : walker.log()){
            log += "\n  " + line;
        }
        return std::format(
            "Expected failure \"{}\" after {} warp(s); got \"{}\" after {}:{}",
            expected, m_case.expectation.warps, failure, sim.state().warps, log
        );
    }

private:
    WarpFadeCase m_case;
};

} // namespace


const WarpFadeCase WARP_FADE_CASES[] = {
    {
        .input = {.name = "a fade over before the hold ends, watched from its start", .fade = 200ms},
        .expectation = {.failure = ""},
    },
    {
        .input = {.name = "a fade over before the hold ends, watched from its end", .fade = 200ms, .watch_along_hold = false},
        .expectation = {.failure = "no fade in and out after walking onto the warp to ViridianCity"},
    },
    {
        .input = {.name = "a fade still on when the hold ends, watched from its end", .fade = 1000ms, .watch_along_hold = false},
        .expectation = {.failure = ""},
    },
    {
        .input = {.name = "black without a warp, destination confirmed", .fade = 1000ms, .black_without_warp = {1}},
        .expectation = {.failure = "the warp to ViridianCity not confirmed", .warps = 0},
    },
    {
        //  00.31 run 4 (console B, 2026-11-10 01:44): taken as walked.
        .input = {.name = "black without a warp, any black taken as the warp", .fade = 1000ms, .black_without_warp = {1}, .confirm_destination = false},
        .expectation = {.failure = "", .warps = 0},
    },
};

void add_tests_OverworldWarpFade(UnitTestDatabase& database){
    for (const WarpFadeCase& item : WARP_FADE_CASES){
        database.add<Test_OverworldWarpFade>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
