/*  Overworld Facing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which way the player faces, read from the screen, and turning them with taps that are checked.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldFacing_H
#define PokemonAutomation_PokemonFRLG_OverworldFacing_H

#include <optional>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonFRLG_OverworldTiles.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace OverworldWalkSteps{

//  The way the player faces, read from the screen; nothing if it can't be read.
std::optional<Direction> read_facing(ConsoleHandle& console, ProControllerContext& context);

//  Turns the player to `direction` with a tap (TURN_TAP), then reads the
//  facing from the screen and taps again while it shows another way, up to
//  MAX_TURN_TAPS taps. A tap the game misses leaves the player facing the
//  old way, and a leg's hold then spends its first 8 frames turning
//  (pokefirered PlayerTurnInPlace(), a fast walk in place): a one-tile
//  hold only turns the player, with nothing to show it (console B,
//  2026-11-09, 00.28 run 2: the step north onto Rock Tunnel B1F row 34).
//  A facing that can't be read is taken as turned: a tap while facing the
//  way already would take a step.
void turn_to(ConsoleHandle& console, ProControllerContext& context, Direction direction);

} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
