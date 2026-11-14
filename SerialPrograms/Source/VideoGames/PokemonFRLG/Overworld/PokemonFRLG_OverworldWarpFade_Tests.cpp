/*  Overworld Warp Fade Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A warp whose fade is over before the hold onto it ends, on the sim: the
 *  walker must watch for the fade from the start of the hold.
 *
 */

#include <format>
#include <functional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
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
    //  SimWalkerRules::scene_confirms_missed_fade.
    bool scene_confirms = true;
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

//  07.02 (console B, 2026-11-09 01:41), the warp to CeruleanCity: "no fade
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
        rules.scene_confirms_missed_fade = m_case.input.scene_confirms;
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

//  A store floor: a pattern that repeats every 16-pixel tile.
ImageRGB32 store_floor(){
    ImageRGB32 image(240, 160);
    for (size_t y = 0; y < 160; y++){
        for (size_t x = 0; x < 240; x++){
            image.pixel(x, y) = (x % 16 < 8) ^ (y % 16 < 8) ? 0xffc0b090 : 0xff906850;
        }
    }
    return image;
}

//  The Department Store's lift room as the console shows it (console B,
//  2026-11-12 00:39): black but for its lit floor, 4 by 3 tiles, in the
//  middle.
ImageRGB32 lift_room(){
    ImageRGB32 image(240, 160);
    for (size_t y = 0; y < 160; y++){
        for (size_t x = 0; x < 240; x++){
            const bool lit = x >= 88 && x < 152 && y >= 56 && y < 104;
            image.pixel(x, y) = lit ? ((x % 16 < 8) ? 0xffd8d0b8 : 0xff807060) : 0xff000000;
        }
    }
    return image;
}

ImageRGB32 black_screen(){
    ImageRGB32 image(240, 160);
    for (size_t y = 0; y < 160; y++){
        for (size_t x = 0; x < 240; x++){
            image.pixel(x, y) = 0xff000000;
        }
    }
    return image;
}

//  10.01 lift_to_1f: the step north into the lift, its fade missed. A
//  watch for the fade started on the lift room never sees black end; the
//  scene after it is taken as the warp, and a black screen or the floor
//  from before the step is not.
UnitTestResult test_lift_room_arrival(){
    const ImageFloatBox game(0, 0, 1, 1);
    RouteStep step;
    step.direction = Direction::NORTH;
    step.tiles = 1;
    step.warps = true;
    step.end.map = "CeladonCity_DepartmentStore_Elevator";
    const ImageRGB32 before = store_floor();
    const ImageRGB32 lift = lift_room();
    const ImageRGB32 black = black_screen();
    BlackScreenOverWatcher late_watch(COLOR_RED);
    for (int frame = 0; frame < 10; frame++){
        late_watch.process_frame(lift, WallClock{} + Milliseconds(100 * frame));
    }
    if (late_watch.black_is_over(lift)){
        return "Expected a fade watch started on the lift room not to see black end.";
    }
    if (!OverworldWalkSteps::arrived_without_fade(before, lift, lift, game, step)){
        return "Expected the lift room after the step to be taken as the warp.";
    }
    if (OverworldWalkSteps::arrived_without_fade(before, black, black, game, step)){
        return "Expected a black screen (the fade still on) not to be taken as the warp.";
    }
    if (OverworldWalkSteps::arrived_without_fade(before, before, before, game, step)){
        return "Expected the floor from before the step not to be taken as the warp.";
    }
    if (OverworldWalkSteps::arrived_without_fade(before, black, lift, game, step)){
        return "Expected the lift room still fading in (black a poll before) not to be taken yet.";
    }
    return true;
}

struct ArrivalScreenInput{
    std::string name;
    std::function<UnitTestResult()> body;
};

struct ArrivalScreenExpectation{
};

struct ArrivalScreenCase{
    ArrivalScreenInput input;
    ArrivalScreenExpectation expectation;
};

class Test_ArrivalScreen : public UnitTest{
public:
    explicit Test_ArrivalScreen(ArrivalScreenCase test_case)
        : UnitTest("PokemonFRLG::OverworldWarpFade - " + test_case.input.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        return m_case.input.body();
    }

private:
    ArrivalScreenCase m_case;
};

const ArrivalScreenCase ARRIVAL_SCREEN_CASES[] = {
    {.input = {"the lift room after a missed fade, by its screen", test_lift_room_arrival}, .expectation = {}},
};

} // namespace


const WarpFadeCase WARP_FADE_CASES[] = {
    {
        .input = {.name = "a fade over before the hold ends, watched from its start", .fade = 200ms},
        .expectation = {.failure = ""},
    },
    {
        .input = {.name = "a fade over before the hold ends, watched from its end", .fade = 200ms, .watch_along_hold = false, .scene_confirms = false},
        .expectation = {.failure = "no fade in and out after walking onto the warp to ViridianCity"},
    },
    {
        //  10.01 lift_to_1f (console B, 2026-11-12 00:39): the step into
        //  the Department Store's lift, its fade missed; the lift room after
        //  it is another scene, so the warp is taken without the fade.
        .input = {.name = "a fade missed, the scene after it confirms the warp", .fade = 200ms, .watch_along_hold = false},
        .expectation = {.failure = ""},
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
        //  09.01 run 4 (console B, 2026-11-10 01:44): taken as walked.
        .input = {.name = "black without a warp, any black taken as the warp", .fade = 1000ms, .black_without_warp = {1}, .confirm_destination = false},
        .expectation = {.failure = "", .warps = 0},
    },
};

void add_tests_OverworldWarpFade(UnitTestDatabase& database){
    for (const WarpFadeCase& item : WARP_FADE_CASES){
        database.add<Test_OverworldWarpFade>(item);
    }
    for (const ArrivalScreenCase& item : ARRIVAL_SCREEN_CASES){
        database.add<Test_ArrivalScreen>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
