/*  Overworld Facing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which way the player faces, read from the screen, and turning them with taps that are checked.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldFacing_H
#define PokemonAutomation_PokemonFRLG_OverworldFacing_H

#include <functional>
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

//  Where a step starts, as far as the route knows: the way the player
//  faces and the tile they stand on.
struct StepStart{
    std::optional<Direction> facing;
    std::optional<MapPosition> at;
};

//  The player's sprite while turning: the walking one, which
//  read_player_facing() knows, or a surfing or biking one, which it doesn't.
enum class TurnSprite{
    WALKING,
    NOT_READ,
};

//  Reads of the facing after a turn tap, about 100 ms apart (about 2 s in
//  all), before turn_to() judges the facing unread: a wanderer that steps
//  over the player moves on in that time.
inline constexpr size_t TURN_FACING_READS = 20;

//  How a turn's taps ended: the facing the last tap's reads showed (none
//  if no read showed one), and the taps sent.
struct TurnTaps{
    std::optional<Direction> facing;
    size_t taps = 0;
};

//  turn_to()'s taps, with the console or a test behind `tap` and `read`
//  (the `read`th read after a tap, from 1): a tap, then up to `reads`
//  reads until one shows a facing, again while it shows another way than
//  `direction`, up to MAX_TURN_TAPS taps. A tap after a facing that never
//  showed isn't sent: it would take a step if the player turned.
TurnTaps tap_until_facing(
    Direction direction, size_t reads,
    const std::function<void()>& tap,
    const std::function<std::optional<Direction>(size_t read)>& read
);

//  What turn_to() knows of a turn tap whose facing never read.
struct UnreadTurn{
    //  What follows the tap: a hold to a wall, not onto a warp, or
    //  anything else.
    enum class Then{
        OTHER,
        HOLD_TO_WALL,
    };
    //  What follows the turn tap before `step`.
    static Then then_for(const RouteStep& step);

    TurnSprite sprite = TurnSprite::WALKING;
    //  The route knows the player's tile.
    bool at_known = false;
    //  The overworld shows with nothing over it: no box, menu or fade.
    bool overworld_clear = false;
    //  What follows the tap.
    Then after = Then::OTHER;
};

//  Whether such a turn tap is taken as turned: always for a NOT_READ
//  sprite; for the walking one when the overworld is clear and either the
//  tile is known or a hold to a wall follows. The sprite is hidden there
//  by what the game draws over it: an escalator's rail, a roof's eave
//  (Cinnabar, north of the Center: console B, 2026-11-13 11:06, 12.03
//  surf_route_21, only the head showing), a tree or a wanderer. The leg's
//  walk then checks the turn: a hold from the old facing turns first. A
//  hold to a wall also needs no known tile: it walks on along its way if
//  the tap stepped, and ends at the wall from any tile of its line, where
//  the wall's check places the player. Pokemon Mansion B1F's row 31 hides
//  the legs under the partition's base, and the hold that found the player
//  there, "east 1 (to wall)" from an unknown tile, failed at its turn
//  (13.03, console B, 2026-11-14 04:43).
bool unread_turn_taken(const UnreadTurn& turn);

//  Turns `player` to `direction` with a tap (TURN_TAP), then reads the
//  facing from the screen and taps again while it shows another way, up to
//  MAX_TURN_TAPS taps. A tap the game misses leaves the player facing the
//  old way, and a leg's hold then spends its first 8 frames turning
//  (pokefirered PlayerTurnInPlace(), a fast walk in place): a one-tile
//  hold only turns the player, with nothing to show it (console B,
//  2026-11-09, 08.01 run 2: the step north onto Rock Tunnel B1F row 34).
//  A facing that can't be read for TURN_FACING_READS reads (about 2 s) is
//  not tapped again, as a tap while facing the way already would take a
//  step. It is taken as turned as unread_turn_taken() says (Fuchsia's
//  Center (0, 5), the escalator's rail over the legs: console B,
//  2026-11-13 00:39, 12.03), else the leg fails with the screenshot, and a
//  re-anchor finds the player.
void turn_to(
    ConsoleHandle& console, ProControllerContext& context, Direction direction, PlayerGender player,
    TurnSprite sprite, const std::optional<MapPosition>& at, UnreadTurn::Then after = UnreadTurn::Then::OTHER
);

} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
