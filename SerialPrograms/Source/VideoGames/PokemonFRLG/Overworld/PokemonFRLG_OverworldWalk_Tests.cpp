/*  Overworld Walk Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdint>
#include <format>
#include <functional>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTest.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "PokemonFRLG_OverworldFacing.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldRecoveryWalk.h"
#include "PokemonFRLG_OverworldSim.h"
#include "PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_BikeSpinMechanics.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using namespace MovementMechanics;
using namespace OverworldWalkSteps;


namespace{

struct OverworldWalkInput{
    MoveSpeed speed;
    int tiles;
    bool to_wall;
    bool surfing;
    bool walk_only = false;
};

struct OverworldWalkExpectation{
    Milliseconds hold;
    std::string_view note;
    bool runs = false;
};

struct OverworldWalkTestCase{
    std::string_view name;
    OverworldWalkInput input;
    OverworldWalkExpectation expectation;
};

//  Whether `mover` is hold_dpad_running(), which holds B.
bool holds_b(const Pokemon::LegMover<ProControllerContext>& mover){
    using Mover = void (*)(ProControllerContext&, Direction, Milliseconds);
    const Mover* target = mover.target<Mover>();
    return target != nullptr && *target == hold_dpad_running;
}

class Test_OverworldWalk : public UnitTest{
public:
    explicit Test_OverworldWalk(OverworldWalkTestCase scenario)
        : UnitTest("PokemonFRLG::OverworldWalk - hold: " + std::string(scenario.name))
        , m_scenario(std::move(scenario))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        RouteStep step;
        step.direction = Direction::WEST;
        step.tiles = m_scenario.input.tiles;
        step.to_wall = m_scenario.input.to_wall;
        step.surfing = m_scenario.input.surfing;
        step.walk_only = m_scenario.input.walk_only;
        const Milliseconds hold = leg_hold_time(step, m_scenario.input.speed);
        if (hold != m_scenario.expectation.hold){
            return std::format("Expected a hold of {}ms, got {}ms.", m_scenario.expectation.hold.count(), hold.count());
        }
        const std::string note = speed_note(step, m_scenario.input.speed);
        if (note != m_scenario.expectation.note){
            return "Expected the note \"" + std::string(m_scenario.expectation.note) + "\", got \"" + note + "\".";
        }
        if (holds_b(leg_mover(step, m_scenario.input.speed)) != m_scenario.expectation.runs){
            return m_scenario.expectation.runs ? "Expected the leg held with B." : "Expected the leg held without B.";
        }
        return true;
    }

private:
    OverworldWalkTestCase m_scenario;
};

//  An open leg lets go half a tile before its last step would end; a leg
//  to a wall is held two tiles longer. Walking is 267ms a tile, running and
//  surfing 133ms.
const OverworldWalkTestCase OVERWORLD_WALK_CASES[] = {
    {
        .name = "walk, open",
        .input = {.speed = MoveSpeed::WALK, .tiles = 5, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(4 * 267 + 133), .note = ""},
    },
    {
        .name = "walk, to a wall",
        .input = {.speed = MoveSpeed::WALK, .tiles = 5, .to_wall = true, .surfing = false},
        .expectation = {.hold = Milliseconds(7 * 267), .note = "[Speed] walk west: 5 tiles in 1335 ms, held 1869 ms"},
    },
    {
        .name = "run, open",
        .input = {.speed = MoveSpeed::RUN, .tiles = 5, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(4 * 133 + 66), .note = "", .runs = true},
    },
    {
        .name = "run, to a wall",
        .input = {.speed = MoveSpeed::RUN, .tiles = 40, .to_wall = true, .surfing = false},
        .expectation = {.hold = Milliseconds(42 * 133), .note = "[Speed] run west: 40 tiles in 5320 ms, held 5586 ms", .runs = true},
    },
    {
        .name = "run, indoors: walks",
        .input = {.speed = MoveSpeed::RUN, .tiles = 5, .to_wall = true, .surfing = false, .walk_only = true},
        .expectation = {.hold = Milliseconds(7 * 267), .note = "[Speed] walk west: 5 tiles in 1335 ms, held 1869 ms"},
    },
    {
        .name = "run, indoors, open: walks",
        .input = {.speed = MoveSpeed::RUN, .tiles = 5, .to_wall = false, .surfing = false, .walk_only = true},
        .expectation = {.hold = Milliseconds(4 * 267 + 133), .note = ""},
    },
    {
        .name = "run, surfing to a wall",
        .input = {.speed = MoveSpeed::RUN, .tiles = 3, .to_wall = true, .surfing = true},
        .expectation = {.hold = Milliseconds(5 * 133), .note = ""},
    },
    {
        .name = "walk, surfing",
        .input = {.speed = MoveSpeed::WALK, .tiles = 3, .to_wall = false, .surfing = true},
        .expectation = {.hold = Milliseconds(2 * 133 + 66), .note = ""},
    },
    //  Console B, 2026-11-09: on the bike, 1450ms moved 15 tiles and 50ms
    //  (a tap) one; 1736ms, held for "west 16 (open)", went past 16.
    {
        .name = "bike, open 16",
        .input = {.speed = MoveSpeed::BIKE, .tiles = 16, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(1550), .note = ""},
    },
    {
        .name = "bike, open 1",
        .input = {.speed = MoveSpeed::BIKE, .tiles = 1, .to_wall = false, .surfing = false},
        .expectation = {.hold = Milliseconds(50), .note = ""},
    },
};


struct TurnTapInput{
    //  SimScript::dropped_turn_taps.
    std::set<int> dropped_turn_taps;
    //  SimWalkerRules::confirm_turn.
    bool confirm_turn = true;
    //  On the Bicycle (SimScript::on_bike, SimWalkerRules::speed).
    bool on_bike = false;
    //  The leg: WALK west 1, or FACE west.
    RouteAction action = RouteAction::WALK;
    //  SimScript::hidden_facing_reads.
    size_t hidden_facing_reads = 0;
};

struct TurnTapExpectation{
    //  Where the leg ends.
    MapPosition end;
    //  The way the player faces after it.
    Direction facing = Direction::WEST;
};

struct TurnTapCase{
    std::string name;
    TurnTapInput input;
    TurnTapExpectation expectation;
};

const MapPosition TURN_START{"ViridianForest", {13, 28}};
const MapPosition TURN_END{"ViridianForest", {12, 28}};

//  Console B, 2026-11-09 (08.01 run 2): the game missed the turn tap before
//  a one-tile open leg, the leg's hold only turned the player, and the leg
//  was taken as walked. On the sim, a leg west 1 for a player facing north.
class Test_OverworldTurnTap : public UnitTest{
public:
    explicit Test_OverworldTurnTap(TurnTapCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - turn tap: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        static const std::vector<MapData> MAPS = overworld_maps();
        RouteStep west;
        west.action = m_case.input.action;
        west.direction = Direction::WEST;
        west.tiles = m_case.input.action == RouteAction::FACE ? 0 : 1;
        west.facing_after = Direction::WEST;
        west.end = m_case.input.action == RouteAction::FACE ? TURN_START : TURN_END;
        SimScript script;
        script.dropped_turn_taps = m_case.input.dropped_turn_taps;
        script.hidden_facing_reads = m_case.input.hidden_facing_reads;
        SimWalkerRules rules;
        rules.confirm_turn = m_case.input.confirm_turn;
        if (m_case.input.on_bike){
            script.on_bike = true;
            script.timing.tile = BIKE_TILE_TIME;
            rules.speed = MoveSpeed::BIKE;
        }
        OverworldSim sim(MAPS, TURN_START, Direction::NORTH, script);
        OverworldSimWalker walker(sim, rules);
        const RoutePlanning planning{.maps = &MAPS, .costs = default_route_costs(), .start = TURN_START, .label = "Sim"};
        const std::string failure = walker.run({west}, planning);
        const SimState& state = sim.state();
        if (failure.empty() && state.position == m_case.expectation.end && state.facing == m_case.expectation.facing){
            return true;
        }
        return std::format(
            "Expected the leg to end on {} facing {}; ended on {} facing {}, failure \"{}\".",
            to_string(m_case.expectation.end), Pokemon::direction_name(m_case.expectation.facing),
            to_string(state.position), Pokemon::direction_name(state.facing), failure
        );
    }

private:
    TurnTapCase m_case;
};

const TurnTapCase TURN_TAP_CASES[] = {
    {.name = "taken", .input = {}, .expectation = {.end = TURN_END}},
    {.name = "dropped, tapped again", .input = {.dropped_turn_taps = {1}}, .expectation = {.end = TURN_END}},
    {.name = "dropped twice, tapped again", .input = {.dropped_turn_taps = {1, 2}}, .expectation = {.end = TURN_END}},
    //  Unchecked, the leg's hold only turns the player; the screen didn't
    //  move, so the leg is held again and walks (judged since console B,
    //  2026-11-12, 12.03 pallet_shore).
    {.name = "dropped, not checked: the hold that only turned is held again", .input = {.dropped_turn_taps = {1}, .confirm_turn = false}, .expectation = {.end = TURN_END}},
    //  Console B, 2026-11-09 (08.03): on the Bicycle the turn tap took a
    //  step, each one-tile leg went two, and the walk to Route 8 ended on
    //  Route 12.
    {.name = "on the bike, no turn tap", .input = {.on_bike = true}, .expectation = {.end = TURN_END}},
    //  Console B, 2026-11-11 (09.02 moon_stone): the game missed the tap of
    //  "face south" on Rocket Hideout B2F, the player still faced north, and
    //  A found nothing to pick up.
    {.name = "face, taken", .input = {.action = RouteAction::FACE}, .expectation = {.end = TURN_START}},
    {.name = "face, dropped, tapped again", .input = {.dropped_turn_taps = {1}, .action = RouteAction::FACE}, .expectation = {.end = TURN_START}},
    //  Console B, 2026-11-13 11:06 (12.03 surf_route_21): north of
    //  Cinnabar's Center the roof's eave hid all but the player's head, the
    //  facing never read after the turn tap west, and the leg failed. A
    //  wanderer over the sprite steps off within the reads.
    {.name = "facing hidden for 5 reads, then read", .input = {.hidden_facing_reads = 5}, .expectation = {.end = TURN_END}},
    {.name = "facing never shows: taken as turned", .input = {.hidden_facing_reads = SIZE_MAX}, .expectation = {.end = TURN_END}},
    {.name = "face, facing never shows: taken as turned", .input = {.action = RouteAction::FACE, .hidden_facing_reads = SIZE_MAX}, .expectation = {.end = TURN_START}},
};

struct UnreadTurnInput{
    OverworldWalkSteps::UnreadTurn turn = {.at_known = true, .overworld_clear = true};
};
struct UnreadTurnExpectation{
    bool taken;
};
struct UnreadTurnCase{
    std::string name;
    UnreadTurnInput input;
    UnreadTurnExpectation expectation;
};

//  OverworldWalkSteps::unread_turn_taken(): a turn whose facing never read
//  is taken where the route knows the tile and nothing covers the
//  overworld; else the leg fails for a re-anchor.
class Test_OverworldUnreadTurn : public UnitTest{
public:
    explicit Test_OverworldUnreadTurn(UnreadTurnCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - unread turn: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        if (OverworldWalkSteps::unread_turn_taken(m_case.input.turn) == m_case.expectation.taken){
            return true;
        }
        return std::format("Expected {}.", m_case.expectation.taken ? "taken as turned" : "the leg to fail");
    }

private:
    UnreadTurnCase m_case;
};

const UnreadTurnCase UNREAD_TURN_CASES[] = {
    {.name = "tile known, overworld clear (a roof's eave)", .input = {}, .expectation = {.taken = true}},
    {.name = "tile unknown", .input = {.turn = {.at_known = false, .overworld_clear = true}}, .expectation = {.taken = false}},
    {.name = "a box or a fade over the overworld", .input = {.turn = {.at_known = true, .overworld_clear = false}}, .expectation = {.taken = false}},
    {.name = "surfing, tile unknown", .input = {.turn = {.sprite = OverworldWalkSteps::TurnSprite::NOT_READ}}, .expectation = {.taken = true}},
};


struct TapMechanicsInput{
    //  The player's facing at the start.
    Direction facing = Direction::NORTH;
    //  The presses, on the sim.
    std::function<void(OverworldSim&)> presses;
};

struct TapMechanicsExpectation{
    //  Tiles stepped east.
    int steps = 0;
    Direction facing = Direction::EAST;
    //  Step taps the game ignored.
    int ignored = 0;
};

struct TapMechanicsCase{
    std::string name;
    TapMechanicsInput input;
    TapMechanicsExpectation expectation;
};

//  Open floor east of the start: where the taps were measured (console B,
//  2026-11-09, MovementMechanics).
const MapPosition TAP_START{"Route10_PokemonCenter_1F", {3, 4}};

//  The sim's taps east, as the console moved the player for them.
class Test_OverworldTapMechanics : public UnitTest{
public:
    explicit Test_OverworldTapMechanics(TapMechanicsCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - tap: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        static const std::vector<MapData> MAPS = overworld_maps();
        OverworldSim sim(MAPS, TAP_START, m_case.input.facing);
        m_case.input.presses(sim);
        const SimState& state = sim.state();
        const TapMechanicsExpectation& expected = m_case.expectation;
        const MapPosition end{TAP_START.map, {TAP_START.point.x + expected.steps, TAP_START.point.y}};
        if (state.position == end && state.facing == expected.facing && state.step_taps_ignored == expected.ignored){
            return true;
        }
        return std::format(
            "Expected {} step(s) east to {} facing {}, {} tap(s) ignored; on {} facing {}, {} ignored.",
            expected.steps, to_string(end), Pokemon::direction_name(expected.facing), expected.ignored,
            to_string(state.position), Pokemon::direction_name(state.facing), state.step_taps_ignored
        );
    }

private:
    TapMechanicsCase m_case;
};

//  `count` taps east of `duration`, each released for `release`.
std::function<void(OverworldSim&)> taps_east(int count, Milliseconds duration, Milliseconds release){
    return [=](OverworldSim& sim){
        for (int c = 0; c < count; c++){
            sim.tap(Direction::EAST, duration, release);
        }
    };
}

//  A turn tap east and a 1000ms wait, then `count` taps east of 66ms,
//  each released for `release`.
std::function<void(OverworldSim&)> turn_then_taps(int count, Milliseconds release){
    return [=](OverworldSim& sim){
        sim.tap(Direction::EAST, TURN_TAP, Milliseconds(1000));
        taps_east(count, Milliseconds(66), release)(sim);
    };
}

//  AutoStory's step_once() east `count` times.
std::function<void(OverworldSim&)> step_once_east(int count){
    return [=](OverworldSim& sim){
        for (int c = 0; c < count; c++){
            sim_step_once(sim, Direction::EAST);
        }
    };
}

const TapMechanicsCase TAP_MECHANICS_CASES[] = {
    {.name = "133ms facing another way only turns", .input = {.presses = taps_east(1, LONGEST_TAP, Milliseconds(700))}, .expectation = {}},
    {.name = "16ms facing that way steps once", .input = {.facing = Direction::EAST, .presses = taps_east(1, Milliseconds(16), Milliseconds(700))}, .expectation = {.steps = 1}},
    {.name = "3 step taps 566ms apart take 2 steps", .input = {.presses = turn_then_taps(3, Milliseconds(500))}, .expectation = {.steps = 2, .ignored = 1}},
    {.name = "3 step taps 666ms apart take 3 steps", .input = {.presses = turn_then_taps(3, Milliseconds(600))}, .expectation = {.steps = 3}},
    {.name = "3 step taps with no release are one hold", .input = {.presses = turn_then_taps(3, Milliseconds(0))}, .expectation = {.steps = 1}},
    {.name = "step_once 3 times takes 3 steps", .input = {.presses = step_once_east(3)}, .expectation = {.steps = 3}},
};

std::string_view movement_name(HoldMovement movement){
    switch (movement){
    case HoldMovement::UNMEASURED:
        return "unmeasured";
    case HoldMovement::MOVED:
        return "moved";
    case HoldMovement::NONE:
        return "no move";
    }
    return "?";
}

//  Where a screen shows black past the map's edge.
enum class OffMap{
    NONE,
    TOP,        //  the top 240 of 640 rows
    BOTTOM,     //  the bottom 240 of 640 rows
    ALL,        //  the whole screen
};

//  One shot of a screen for hold_scroll_change().
struct HoldShot{
    //  How far the camera has moved, in pixels, right and down.
    size_t scroll_x = 0;
    size_t scroll_y = 0;
    //  A town's name banner over the top left, as the game shows on
    //  entering a town.
    bool banner = false;
    OffMap off_map = OffMap::NONE;
};

//  A 960x640 screen of 64-pixel tiles, each its own color, seen as `shot`.
ImageRGB32 tile_map(const HoldShot& shot){
    ImageRGB32 image(960, 640);
    for (size_t y = 0; y < image.height(); y++){
        for (size_t x = 0; x < image.width(); x++){
            const uint32_t tile = (uint32_t)((x + shot.scroll_x) / 64 * 7 + (y + shot.scroll_y) / 64 * 13);
            const bool in_banner = shot.banner && x < 400 && y < 80;
            const bool off_map = shot.off_map == OffMap::ALL
                || (shot.off_map == OffMap::TOP && y < 240)
                || (shot.off_map == OffMap::BOTTOM && y >= 400);
            if (off_map){
                image.pixel(x, y) = 0xff070707;
            }else{
                image.pixel(x, y) = in_banner ? 0xfff8f8f8 : 0xff000000 | (tile * 0x2f1b3d & 0x00ffffff);
            }
        }
    }
    return image;
}

struct HoldScrollInput{
    HoldShot before;
    HoldShot after;
};
struct HoldScrollExpectation{
    HoldMovement movement;
};
struct HoldScrollCase{
    std::string name;
    HoldScrollInput input;
    HoldScrollExpectation expectation;
};

//  hold_scroll_change() over drawn screens: a hold reads as moved when the
//  map scrolled in every edge band that shows it; a banner or a still map
//  is no move.
class Test_OverworldHoldScroll : public UnitTest{
public:
    explicit Test_OverworldHoldScroll(HoldScrollCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - hold scroll: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ImageFloatBox game(0, 0, 1, 1);
        const std::optional<double> change = hold_scroll_change(tile_map(m_case.input.before), tile_map(m_case.input.after), game);
        const HoldMovement movement = hold_movement(change);
        if (movement == m_case.expectation.movement){
            return true;
        }
        return std::format(
            "Expected {}; read {} (changed by {:.1f}).",
            movement_name(m_case.expectation.movement), movement_name(movement), change.value_or(0)
        );
    }

private:
    HoldScrollCase m_case;
};

const HoldScrollCase HOLD_SCROLL_CASES[] = {
    //  Pallet Town after a Fly (console B, 2026-11-13 02:42 and 02:46,
    //  12.03): the first hold walked nothing while the town's banner slid in
    //  over the top of the screen, and was taken as walked ("changed by
    //  223.0"); every later tile was then 2 rows off.
    {.name = "a town's banner over a still map", .input = {.before = {}, .after = {.banner = true}}, .expectation = {.movement = HoldMovement::NONE}},
    {.name = "a step south", .input = {.before = {}, .after = {.scroll_y = 64}}, .expectation = {.movement = HoldMovement::MOVED}},
    //  Pokemon Mansion 1F's bottom row (console B, 2026-11-14 03:20, 13.02):
    //  a walk east of 13 tiles read as no move, the black below the map the
    //  same in both shots.
    {
        .name = "a step east along the map's bottom edge",
        .input = {.before = {.off_map = OffMap::BOTTOM}, .after = {.scroll_x = 64, .off_map = OffMap::BOTTOM}},
        .expectation = {.movement = HoldMovement::MOVED},
    },
    {
        .name = "a step east along the map's top edge",
        .input = {.before = {.off_map = OffMap::TOP}, .after = {.scroll_x = 64, .off_map = OffMap::TOP}},
        .expectation = {.movement = HoldMovement::MOVED},
    },
    {
        .name = "standing at the map's bottom edge",
        .input = {.before = {.off_map = OffMap::BOTTOM}, .after = {.off_map = OffMap::BOTTOM}},
        .expectation = {.movement = HoldMovement::NONE},
    },
    {
        .name = "a town's banner at the map's bottom edge",
        .input = {.before = {.off_map = OffMap::BOTTOM}, .after = {.banner = true, .off_map = OffMap::BOTTOM}},
        .expectation = {.movement = HoldMovement::NONE},
    },
    {
        .name = "no map on the screen",
        .input = {.before = {.off_map = OffMap::ALL}, .after = {.off_map = OffMap::ALL}},
        .expectation = {.movement = HoldMovement::UNMEASURED},
    },
};

struct HoldFramesInput{
    //  Console B frames (1920x1080) in CommandLineTests.
    std::string before;
    std::string after;
};
struct HoldFramesCase{
    std::string name;
    HoldFramesInput input;
    HoldScrollExpectation expectation;
};

//  hold_scroll_change() over console frames, in the console's game box.
class Test_OverworldHoldFrames : public UnitTest{
public:
    explicit Test_OverworldHoldFrames(HoldFramesCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - hold frames: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        const ImageFloatBox game(0.09375, 0.00462963, 0.8125, 0.962963);
        const ImageRGB32 before(UNIT_TEST_RESOURCE_PATH() + m_case.input.before);
        const ImageRGB32 after(UNIT_TEST_RESOURCE_PATH() + m_case.input.after);
        const std::optional<double> change = hold_scroll_change(before, after, game);
        const HoldMovement movement = hold_movement(change);
        if (movement == m_case.expectation.movement){
            return true;
        }
        return std::format(
            "Expected {}; read {} (changed by {:.1f}).",
            movement_name(m_case.expectation.movement), movement_name(movement), change.value_or(0)
        );
    }

private:
    HoldFramesCase m_case;
};

//  Pokemon Mansion 1F's bottom row (console B, 2026-11-14 03:19-03:20,
//  13.02): "east 13 (to wall)" from (17, 33) walked to (30, 33) but read as
//  no move; the route walked the leg again from (17, 33), which moved
//  nothing, and failed.
const HoldFramesCase HOLD_FRAMES_CASES[] = {
    {
        .name = "Pokemon Mansion 1F's bottom row, east 13",
        .input = {.before = "PokemonFRLG/OverworldHold/PokemonMansion1F_Row33_Before.png", .after = "PokemonFRLG/OverworldHold/PokemonMansion1F_Row33_East13.png"},
        .expectation = {.movement = HoldMovement::MOVED},
    },
    {
        .name = "Pokemon Mansion 1F's bottom row, held again at the wall",
        .input = {.before = "PokemonFRLG/OverworldHold/PokemonMansion1F_Row33_East13.png", .after = "PokemonFRLG/OverworldHold/PokemonMansion1F_Row33_HeldAgain.png"},
        .expectation = {.movement = HoldMovement::NONE},
    },
};

} // namespace


//  Console B, 2026-11-11 15:36 (09.02 beside_moon_stone): the step tap of
//  "east 1 (open)" from Rocket Hideout B2F (1, 4), facing east already,
//  moved the player but the screen check read no move; the walker tapped
//  again and ended on (3, 4), not (2, 4).
class Test_OverworldUnseenStepTap : public UnitTest{
public:
    Test_OverworldUnseenStepTap()
        : UnitTest("PokemonFRLG::OverworldWalk - step tap read as not moved")
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        static const std::vector<MapData> MAPS = overworld_maps();
        const MapPosition start{"RocketHideout_B2F", {1, 4}};
        const MapPosition end{"RocketHideout_B2F", {2, 4}};
        RouteStep east;
        east.action = RouteAction::WALK;
        east.direction = Direction::EAST;
        east.tiles = 1;
        east.tapped = true;
        east.facing_after = Direction::EAST;
        east.end = end;
        SimScript script;
        script.unseen_step_taps = {1};
        OverworldSim sim(MAPS, start, Direction::EAST, script);
        OverworldSimWalker walker(sim);
        const RoutePlanning planning{.maps = &MAPS, .costs = default_route_costs(), .start = start, .label = "Sim"};
        const std::string failure = walker.run({east}, planning);
        if (sim.state().position == end){
            return true;
        }
        return std::format("Expected the leg to end on {}; ended on {}, failure \"{}\".", to_string(end), to_string(sim.state().position), failure);
    }
};

struct MapEdgeInput{
    //  On Fuchsia's Center, FuchsiaCity_PokemonCenter_1F; -1 counts from
    //  the far side: the last column or row.
    TilePos tile;
};
struct MapEdgeExpectation{
    bool edge;
};
struct MapEdgeCase{
    std::string name;
    MapEdgeInput input;
    MapEdgeExpectation expectation;
};

//  Console B, 2026-11-13 00:39 (12.03): on Fuchsia's Center (0, 5) the
//  escalator's rail hides the player's legs, the facing can't be read after
//  the turn tap east, and the leg failed. turn_to() takes the tap as turned
//  on a map's edge tile, where the route knows the player stands, and fails
//  the leg elsewhere.
class Test_OverworldTurnOnMapEdge : public UnitTest{
public:
    explicit Test_OverworldTurnOnMapEdge(MapEdgeCase test_case)
        : UnitTest("PokemonFRLG::OverworldWalk - turn tap unread on a map's edge: " + test_case.name)
        , m_case(std::move(test_case))
    {}
    virtual UnitTestResult run(Logger&, CancellableScope&) const override{
        static const std::vector<MapData> MAPS = overworld_maps();
        const auto center = std::find_if(MAPS.begin(), MAPS.end(), [](const MapData& map){ return map.name == "FuchsiaCity_PokemonCenter_1F"; });
        if (center == MAPS.end()){
            return "FuchsiaCity_PokemonCenter_1F isn't on the maps.";
        }
        const MapData& CENTER = *center;
        TilePos tile = m_case.input.tile;
        if (tile.x == -1){
            tile.x = CENTER.tiles.width - 1;
        }
        if (tile.y == -1){
            tile.y = CENTER.tiles.height - 1;
        }
        if (on_map_edge(CENTER, tile) == m_case.expectation.edge){
            return true;
        }
        return std::format("({}, {}): expected {}.", tile.x, tile.y, m_case.expectation.edge ? "an edge tile" : "not an edge tile");
    }

private:
    MapEdgeCase m_case;
};

const MapEdgeCase MAP_EDGE_CASES[] = {
    {.name = "west column, beside the escalator (0, 5)", .input = {.tile = {0, 5}}, .expectation = {.edge = true}},
    {.name = "north-west corner tile (0, 2)", .input = {.tile = {0, 2}}, .expectation = {.edge = true}},
    {.name = "east column", .input = {.tile = {-1, 5}}, .expectation = {.edge = true}},
    {.name = "south row", .input = {.tile = {7, -1}}, .expectation = {.edge = true}},
    {.name = "the nurse's tile (7, 4)", .input = {.tile = {7, 4}}, .expectation = {.edge = false}},
    {.name = "a column in (1, 5)", .input = {.tile = {1, 5}}, .expectation = {.edge = false}},
    {.name = "off the map (-2, 5)", .input = {.tile = {-2, 5}}, .expectation = {.edge = false}},
};

void add_tests_OverworldWalk(UnitTestDatabase& database){
    for (const auto& scenario : OVERWORLD_WALK_CASES){
        database.add<Test_OverworldWalk>(scenario);
    }
    for (const TurnTapCase& item : TURN_TAP_CASES){
        database.add<Test_OverworldTurnTap>(item);
    }
    for (const TapMechanicsCase& item : TAP_MECHANICS_CASES){
        database.add<Test_OverworldTapMechanics>(item);
    }
    database.add<Test_OverworldUnseenStepTap>();
    for (const UnreadTurnCase& item : UNREAD_TURN_CASES){
        database.add<Test_OverworldUnreadTurn>(item);
    }
    for (const MapEdgeCase& item : MAP_EDGE_CASES){
        database.add<Test_OverworldTurnOnMapEdge>(item);
    }
    for (const HoldScrollCase& item : HOLD_SCROLL_CASES){
        database.add<Test_OverworldHoldScroll>(item);
    }
    for (const HoldFramesCase& item : HOLD_FRAMES_CASES){
        database.add<Test_OverworldHoldFrames>(item);
    }
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
