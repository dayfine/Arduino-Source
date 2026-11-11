/*  BDSP AutoStory - Leg Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a wild battle on an open leg's taps left the player. The walk
 *  records what its watchers saw, timed (WalkResult::events); the shared
 *  classifier (Pokemon::classify_interrupt()) says what stopped the leg,
 *  and a wild battle is placed by when its first timed event showed: the
 *  tap that set it off, and the taps that moved after it was run from.
 *  Without that the walker took the player to be anywhere on the leg.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LegBattle_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LegBattle_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_Interrupts.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  BDSP's timing for a battle on an open leg's taps, as data.
struct LegBattleTiming{
    Pokemon::InterruptKnobs interrupts = {};
    //  How long after its tap went in each of a wild battle's events shows.
    //  NOT MEASURED on BDSP's console yet: estimates from the walk's own
    //  waits (a tap is held 160 ms, a step takes about 270 ms; the battle's
    //  box shows within WHITE_FLASH_BATTLE_WAIT). Measure them from a lab-B
    //  walk log before trusting a placement to the tile.
    std::vector<Pokemon::EventLag> lags = {};
    //  A tap a tile: walk_tiles_dpad()'s chunk.
    Milliseconds tap_time = Milliseconds(800);
    //  Tiles either side of the placed one the player may be on, for the
    //  unmeasured lags.
    size_t slack = 1;
    //  How long running from a battle takes, for the sim's events.
    Milliseconds escape_time = Milliseconds(6000);
};
//  BDSP's timing.
const LegBattleTiming& leg_battle_timing();

//  Tiles moved along a leg, from `min` to `max`.
struct TileSpan{
    size_t min = 0;
    size_t max = 0;
};

//  What stopped an open leg's taps, and how far along the leg it left the
//  player.
struct LegBattlePlace{
    Pokemon::InterruptVerdict verdict = {};
    //  Set for one wild battle whose step the events time.
    std::optional<TileSpan> moved = {};
};

//  For `walk`, the taps of an open leg of `tiles` tiles that a wild battle
//  interrupted: what its events show stopped the player, and for one wild
//  battle, the tiles moved: the tile its tap reached plus the taps that
//  moved after it, give or take `timing.slack`. Not placed for more than
//  one battle, for an UNSETTLED verdict (a fade too recent to tell a
//  battle from a warp) or any other kind, or for a walk with no events.
LegBattlePlace place_leg_battle(const WalkResult& walk, size_t tiles, const LegBattleTiming& timing = leg_battle_timing());

//  "<label>: wild encounter: +500 ms fade white rose, ...; 4 to 6 tiles
//  in.", or why the battle was not placed.
std::string leg_battle_line(const std::string& label, const WalkResult& walk, const LegBattlePlace& place);

//  The tiles `span` steps toward `direction` reach from each of `from`. A
//  walk stopped before `span.min` by a blocked tile stands on the last
//  tile it reached.
std::vector<Pokemon::TilePos> tiles_moved(
    const StoryWorld& world, const std::vector<Pokemon::TilePos>& from,
    Pokemon::Direction direction, TileSpan span
);

//  Records in `walk` what the console's watchers show for a wild battle
//  that tap `tap` (from 1; 0 for a step before the walk) of `walk` set
//  off, and its escape: for the sim. Times run from `walk.started`, a tap
//  every `timing.tap_time`.
void record_tap_battle(WalkResult& walk, size_t tap, const LegBattleTiming& timing = leg_battle_timing());



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
