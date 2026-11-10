/*  AutoStory Trainer Line
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Steps into a trainer's line of sight a tile at a time, until the
 *  trainer sees the player.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTrainerLine_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTrainerLine_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonFRLG_AutoStoryPresses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Steps with `step` a tile at a time, at most `max_tiles`, until `seen`
//  says a trainer saw the player. Returns the tiles stepped, or nothing
//  if no trainer did. A walk that ended a tile short of a trainer's line
//  still meets them (console B, 2026-11-03, Nugget Bridge: the walk ended
//  on (11, 33), one step reached (11, 32), and Cale on row 31 never saw
//  the player).
std::optional<int> step_until_seen(int max_tiles, const std::function<void()>& step, const std::function<bool()>& seen);

//  How long after a step into a trainer's line their message shows: the
//  "!", then their words, as the route walker watches for a trainer
//  with no tiles to walk up (PokemonFRLG_OverworldRecoveryWalk.cpp).
inline constexpr Milliseconds TRAINER_LINE_WATCH(2500);

//  The variant of a STEP into a trainer's line: a tile at a time, at most
//  TRAINER_LINE_TILES, until the trainer sees the player
//  (step_until_seen()). The battle after it starts on the trainer's row
//  whether the walk before ended on its tile or a tile short.
inline const std::string TRAINER_LINE_VARIANT = "trainer_line";
//  Maximum tiles to step into a trainer's line of sight.
inline constexpr int TRAINER_LINE_TILES = 3;

//  From the overworld: step_until_seen() `direction`, turning first
//  unless `facing` is `direction`, each step watched TRAINER_LINE_WATCH
//  for a trainer's message or battle.
std::optional<int> step_into_line(
    ConsoleHandle& console, ProControllerContext& context,
    Pokemon::Direction direction, std::optional<Pokemon::Direction> facing, int max_tiles
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
