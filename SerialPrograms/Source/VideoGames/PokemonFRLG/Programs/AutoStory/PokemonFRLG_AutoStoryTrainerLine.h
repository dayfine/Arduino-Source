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
//  How long a step into a trainer's line is watched for a wild battle it
//  set off: its HP boxes, with no message before them, came over 5 s after
//  the step in Mt. Moon (UNSEEN_BATTLE_WATCH). The trainer didn't see the
//  player on that step (the game looks for trainers before it rolls for a
//  wild battle), so it is run from and the stepping goes on: in Rock
//  Tunnel the battle step after it found no trainer's message (the sim,
//  2026-11-09).
inline constexpr Milliseconds WILD_BATTLE_IN_LINE_WATCH(5000);

//  How long a step into the line of a trainer who looks around waits for
//  them to turn to the player (TRAINER_WAIT_VARIANT).
inline constexpr Milliseconds TRAINER_WAIT_WATCH(30000);

//  The variant of a STEP into a trainer's line: a tile at a time, at most
//  TRAINER_LINE_TILES, until the trainer sees the player
//  (step_until_seen()). The battle after it starts on the trainer's row
//  whether the walk before ended on its tile or a tile short.
inline const std::string TRAINER_LINE_VARIANT = "trainer_line";
//  Maximum tiles to step into a trainer's line of sight.
inline constexpr int TRAINER_LINE_TILES = 3;

//  The variant of a STEP into the line of a trainer who looks around
//  (MOVEMENT_TYPE_LOOK_AROUND): one tile, then a wait of up to
//  TRAINER_WAIT_WATCH for the trainer to turn to the player. Indoors, where
//  the player can't run, such a trainer turns a random way every 32 to 128
//  frames (pokefirered event_object_movement.c, MovementType_LookAround;
//  it turns to the player only when the player runs), so they face the
//  player within 30 s all but about 1 time in 500; and the game checks
//  their sight every frame (ProcessPlayerFieldInput()), not only after a
//  step. Stepped into from outside their sight onto the second tile of
//  their line, the player is always seen there, and the trainer walks up
//  one tile.
inline const std::string TRAINER_WAIT_VARIANT = "trainer_wait";

//  From the overworld: step_until_seen() `direction`, turning first
//  unless `facing` is `direction`, each step watched `watch` for a
//  trainer's message, and at least WILD_BATTLE_IN_LINE_WATCH for a wild
//  battle, which is run from.
std::optional<int> step_into_line(
    ConsoleHandle& console, ProControllerContext& context,
    Pokemon::Direction direction, std::optional<Pokemon::Direction> facing, int max_tiles,
    Milliseconds watch = TRAINER_LINE_WATCH
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
