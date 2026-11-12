/*  Overworld Late Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a wild battle seen only after an open leg's hold stopped the
 *  player. The walker records the leg's screens as timed events
 *  (Pokemon::ScreenEventLog) through the hold and the watch after it. The
 *  shared classifier (Pokemon::classify_interrupt()) says what stopped the
 *  player, and a wild battle is placed by when its first timed event
 *  showed, not by when the watcher that cleared it fired: that is after the
 *  hold, and the leg could have stopped anywhere along it.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldLateBattle_H
#define PokemonAutomation_PokemonFRLG_OverworldLateBattle_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_Interrupts.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonFRLG_OverworldReanchor.h"
#include "PokemonFRLG_OverworldRoute.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The events of a wild battle FRLG has timed from its step: its text box
//  ("Wild <Pokemon> appeared!", BATTLE_STARTED), LATE_WILD_BATTLE_DELAY
//  after the step on Route 1 (console B, 2026-11-01, run 7). The flashes
//  are what the walker missed, and the wipe isn't timed on the console yet.
const std::vector<Pokemon::EventLag>& wild_battle_lags();

//  The tile of a leg (from 1) that a step begun `since_hold` into its hold
//  reaches: the steps begin a tile apart from the hold's start. At most
//  `tiles`.
int tile_reached(Milliseconds since_hold, Milliseconds tile_time, int tiles);

//  What stopped an open leg after its hold, and where.
struct LateStop{
    Pokemon::InterruptVerdict verdict;
    //  The tiles of the leg the player may be on. Set for a wild battle
    //  whose step the log times, and for a message after a leg that ends
    //  below a sign (RouteStep::sign_ahead): its end.
    std::optional<WalkedTiles> walked;
};

//  For `step`, an open leg held from `hold_start` at `tile_time` a tile,
//  after which something was seen and cleared by `now`: what `log` shows
//  stopped the player there, and for a wild battle, the tile its step
//  reached (tile_reached()), give or take walked_near()'s slack.
LateStop place_late_stop(
    const RouteStep& step, Milliseconds tile_time,
    const Pokemon::ScreenEventLog& log, WallClock hold_start, WallClock now
);

//  "Route: seen after the hold of <step>: wild encounter: +5035 ms battle
//  started rose; 5 to 7 tiles in.", times from `hold_start`.
std::string late_stop_line(const RouteStep& step, const LateStop& stop, WallClock hold_start);


//  Registers the late battle unit tests, on the sim. add_tests_OverworldSim()
//  calls it.
void add_tests_OverworldLateBattle(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
