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
#include "PokemonFRLG_OverworldRouteSteps.h"
#include "PokemonFRLG_OverworldTiles.h"
#include "PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace OverworldWalkSteps{

//  The way `player` faces, read from the screen with their sprites; nothing
//  if it can't be read.
std::optional<Direction> read_facing(ConsoleHandle& console, ProControllerContext& context, PlayerGender player);

//  Whether `tile` is in `map`'s first or last column or row.
bool on_map_edge(const MapData& map, const TilePos& tile);

//  The player's sprite while turning: the walking one, which
//  read_player_facing() knows, or a surfing or biking one, which it doesn't.
enum class TurnSprite{
    WALKING,
    NOT_READ,
};

//  Turns `player` to `direction` with a tap (TURN_TAP), then reads the
//  facing from the screen and taps again while it shows another way, up to
//  MAX_TURN_TAPS taps. A tap the game misses leaves the player facing the
//  old way, and a leg's hold then spends its first 8 frames turning
//  (pokefirered PlayerTurnInPlace(), a fast walk in place): a one-tile
//  hold only turns the player, with nothing to show it (console B,
//  2026-11-09, 08.01 run 2: the step north onto Rock Tunnel B1F row 34).
//  A facing that can't be read for TURN_FACING_READS reads (about 2 s) is
//  not taken as turned, nor is a tap sent again, as one while facing the
//  way already would take a step: the leg fails, with the screenshot, as
//  the player isn't settled and their place is unknown (a re-anchor finds
//  it).
//  Two exceptions take the tap as turned. A NOT_READ sprite's facing
//  can't be read at all. And `at`, where the route knows the player turns,
//  on its map's edge tile: a tile drawn over the player can hide the sprite
//  there (Fuchsia's Center (0, 5), the escalator's rail over the legs;
//  console B, 2026-11-13 00:39, 12.03). Without `at` the leg fails.
void turn_to(
    ConsoleHandle& console, ProControllerContext& context, Direction direction, PlayerGender player,
    TurnSprite sprite, const std::optional<MapPosition>& at
);

} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
