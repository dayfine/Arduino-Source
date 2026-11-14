/*  Overworld Sim State
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The simulated game's script and state, and what the simulated walker
 *  saw: the parts of OverworldSim its tests set up and check.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldSimState_H
#define PokemonAutomation_PokemonFRLG_OverworldSimState_H

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEvents.h"
#include "PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_OverworldSimScreens.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a test sets up beyond the maps.
struct SimScript{
    std::vector<SimEncounter> encounters;
    //  The START menu is open at the start: the D-pad moves its cursor.
    bool start_menu_open = false;
    //  The hold numbered by the key (from 1) stops this many tiles short,
    //  with nothing on screen (the forest drift, console B, 2026-11-01).
    std::map<int, int> short_holds;
    //  Maps where every step can start a wild battle, also on tiles the maps
    //  don't mark as grass: Mt. Moon B2F's tiles closed on one side ('8')
    //  are cave floor too (console B, 2026-11-03).
    std::set<std::string> wild_floor_maps;
    //  Whether the game holds the player up a tile on a step onto a
    //  connected map. It did north onto Nugget Bridge (console B,
    //  2026-11-03, rows 05.05 and 05.09), not south from Route 6 into
    //  Vermilion City (row 06.03).
    bool connections_stall = true;
    //  Wild battles at the game's own odds instead of `encounters`, on the
    //  maps named here, with their land encounter rate (pokefirered
    //  src/data/wild_encounters.json), rolled from `encounter_seed`.
    std::map<std::string, int> encounter_rates;
    uint32_t encounter_seed = 0;
    //  Fault injection: turn taps (taps while the player faces another way)
    //  the game misses, by number from 1; the player goes on facing the old
    //  way. The game drops none from a standstill (MovementMechanics: 70 of
    //  70 turned); these test the walker's check of the turn
    //  (SimWalkerRules::confirm_turn) after 08.01 run 2 (console B,
    //  2026-11-09), where a one-tile leg only turned the player.
    std::set<int> dropped_turn_taps;
    //  Fault injection: walker step taps that moved the player, by number
    //  from 1 among those that moved, which the walker's screen check reads
    //  as not moved. On Rocket Hideout B2F's floor a real step read so, and
    //  the tap again took a second tile (console B, 2026-11-11 15:03 and
    //  15:36, 09.02).
    std::set<int> unseen_step_taps;
    //  Fault injection: turn taps, by number from 1 as for
    //  dropped_turn_taps, that the walker's screen check reads as a step.
    //  The turn south before 10.04's "south 3" read so (console B,
    //  2026-11-12 03:08), and the leg ended a tile short of the stairs.
    std::set<int> seen_turn_taps;
    //  Fault injection: steps onto a warp, by number from 1, that show the
    //  fade's black and clear with the player where they were, not warped
    //  (console B, 2026-11-10 01:44, 09.01 run 4, "east 1 (warp)" on B4F).
    std::set<int> black_without_warp;
    //  Fault injection: a hold that steps onto a spinner ends at the
    //  slide's stop, not walking on from it. A re-anchor hold west from
    //  Rocket Hideout B2F's stop (14, 13) slid to the stop (10, 15) and
    //  stayed there; the legs after it ended a tile short of their wall
    //  (09.02, console B, 2026-11-11 14:25).
    bool holds_end_at_slide_stop = false;
    //  Fault injection: maps whose warps do nothing: a step or press onto
    //  one leaves the player where they were, with no fade, as a wall would
    //  (the warp no-op fault class, PokemonFRLG_AutoStoryFaultSweep_Tests.cpp).
    std::set<std::string> dead_warp_maps;
    //  Fault injection: legs on the Bicycle counted out a tap at a time
    //  (SimState::bike_legs), by number from 1, that start with
    //  the player off the Bicycle: the mount state lost. On 11.01 a second
    //  SELECT took the player off, and every bike tap after only turned
    //  them (console B, 2026-11-12 05:42).
    std::set<int> off_bike_at_legs;
    //  The player rides the Bicycle: a tap facing another way takes a step
    //  too, a hold through a turn loses no tile
    //  (MovementMechanics::BIKE_TAP_ONLY_TURNS), and no step tap is
    //  ignored: a 40ms turn tap and a 56ms hold 340ms after it took two
    //  tiles (console B, 2026-11-09, 08.03). At the start: SELECT gets on
    //  and off (OverworldSim::press_select()), a warp gets off. Each tap is
    //  a tile; one in MovementMechanics::BIKE_FAST_TAP_DROP_EVERY of those
    //  that start sooner than BIKE_TAP_DROP_PERIOD after the last is
    //  dropped. Its pace is timing.bike_tile.
    bool on_bike = false;
    //  The Bicycle is registered (Key Items, REGISTER): SELECT gets on. A
    //  reload of a save made before it was registered loses it.
    bool bike_registered = false;
    //  Caves dark without Flash (Rock Tunnel): the overworld is black but
    //  for a lit circle on the player, which the walker's battle text box
    //  check and screen observer read as a battle on its way (console B,
    //  2026-11-09, 08.01), unless SimWalkerRules::dark_cave_not_battle.
    std::set<std::string> dark_maps;
    //  A lift's map, and where its door lets the player out for each floor
    //  on its panel's menu, top first (pokefirered setdynamicwarp in
    //  RocketHideout_Elevator's scripts): the door's own warp is the
    //  game's placeholder. The hideout's: rocket_hideout_lift_room() under
    //  LIFT_MAP, with rocket_hideout_lift_exits().
    std::map<std::string, std::vector<MapPosition>> lift_floors = {};
    //  The fade out of a lift only dims the screen, never black (console B,
    //  2026-11-10, 00.32 lift_to_b4f): no FADE shows for it.
    bool lift_exit_dims = true;
    //  A trainer's script after their battle, this long: a message, then
    //  black while the game takes the trainer away, and no input until it
    //  ends. Giovanni's on Rocket Hideout B4F (pokefirered
    //  RocketHideout_B4F_EventScript_Giovanni: msgbox, fadescreen,
    //  removeobject): a tap 0.4 s after his battle's text was lost (console
    //  B, 2026-11-11 17:51, 09.02 east_of_silph_scope). Not set: none.
    std::optional<Milliseconds> scene_after_trainer_battle;
    SimTiming timing;
};

//  The player and the screen, and what went wrong that the game would show.
struct SimState{
    MapPosition position;
    Direction facing = Direction::SOUTH;
    SimScreen screen = SimScreen::OVERWORLD;
    //  Whether black shows below the map: the player stands near the bottom
    //  of a map with nothing joined to it (a small room or a gate).
    bool black_below = false;
    //  Whether the player's map is one of SimScript::dark_maps.
    bool dark = false;
    int steps = 0;
    int warps = 0;
    //  The map each warp was taken from, oldest first.
    std::vector<std::string> warped_from;
    int wild_battles = 0;
    //  From each wild battle's step to the overworld after it, together.
    Milliseconds in_wild_battles{0};
    int signs_read = 0;
    int turn_taps_dropped = 0;
    //  Taps that only turned the player, or were dropped turning them.
    int turn_taps = 0;
    //  On the Bicycle (SimScript::on_bike).
    bool on_bike = false;
    //  On the water (SimField::SURF).
    bool surfing = false;
    //  Bicycle taps the game dropped, too soon after the last one.
    int bike_taps_dropped = 0;
    //  Step taps the game ignored, inside the lockout after the last one
    //  (MovementMechanics::STEP_TAP_LOCKOUT).
    int step_taps_ignored = 0;
    //  Walker step taps that moved the player (SimScript::unseen_step_taps).
    int moved_step_taps = 0;
    //  Legs on the Bicycle counted out a tap at a time: taps on it, each
    //  a way other than the last's, or after another press
    //  (SimScript::off_bike_at_legs).
    int bike_legs = 0;
    std::vector<std::string> trainers_beaten;
    //  Things the walker did that the game wouldn't let pass: D-pad holds
    //  under a battle, a trainer's battle fled, a battle fought that wasn't
    //  there.
    std::vector<std::string> violations;
    std::vector<std::string> log;
    //  The sim's clock, from its start.
    Milliseconds time{0};
    //  Each screen as it showed, oldest first, from `screen` at the start.
    std::vector<SimShown> shown;
};

//  A time on the sim's clock as a WallClock, for the shared judges.
WallClock sim_wall_clock(Milliseconds time);

//  The screens `state` showed from `from` until its time, as the walker's
//  screen observer (FrlgScreenObserver) reports them: a message's box, a
//  battle's text box with or without its HP boxes, and the black of a
//  battle's wipe or a warp's fade. A battle's flashes are no uniform
//  screen, and black below a small room, which the observer's battle text
//  box detector may take for a box, isn't modelled.
ScreenEventLog screen_event_log(const SimState& state, Milliseconds from);

//  A watcher that fired: which (an index into the watchers asked for), and
//  how long after the hold or watch started.
struct SimSeen{
    size_t watcher = 0;
    Milliseconds elapsed{0};
};

//  How the walker handles the battle on screen.
enum class SimBattleChoice{
    RUN,
    FIGHT,
};
//  How a simulated battle concluded.
enum class SimBattleEnd{
    ENDED,
    CANT_RUN,       //  a trainer's battle: "No! There's no running from a TRAINER battle!"
    NO_BATTLE,      //  there was none, on screen or about to show
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
