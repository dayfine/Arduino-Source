/*  Overworld Facing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Navigation/Pokemon_Direction.h"
#include "PokemonFRLG_MovementTurnPresses.h"
#include "PokemonFRLG_OverworldMaps.h"
#include "PokemonFRLG_OverworldWalkMoves.h"
#include "PokemonFRLG_OverworldFacing.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
namespace OverworldWalkSteps{

using namespace MovementMechanics;


//  Each leg starts with a turn tap unless it goes the way the player faces.
//  A tap while already facing that way takes a step, so a facing not given
//  is read from the screen. If that fails too it taps anyway.
std::optional<Direction> read_facing(ConsoleHandle& console, ProControllerContext& context, PlayerGender player){
    context.wait_for_all_requests();
    VideoSnapshot screen = console.video().snapshot();
    std::optional<Direction> facing;
    if (screen){
        facing = read_player_facing(screen, player);
    }
    console.log(std::string("Route: the player faces ") + (facing ? Pokemon::direction_name(*facing) : "an unknown way") + ".");
    return facing;
}

bool on_map_edge(const MapData& map, const TilePos& tile){
    if (!map.tiles.contains(tile)){
        return false;
    }
    return tile.x == 0 || tile.y == 0 || tile.x == map.tiles.width - 1 || tile.y == map.tiles.height - 1;
}

namespace{

//  Reads of the facing after a turn tap, TURN_FACING_REREAD apart (about
//  2 s), before the leg fails: a facing that can't be read means the player
//  isn't on screen at rest (a fade, a box, a scene), and a tap again while
//  facing that way already would take a step.
constexpr size_t TURN_FACING_READS = 20;
//  Surfing and biking draw other sprites, which read_player_facing() doesn't
//  know: a few reads, then the old way, taken as turned.
constexpr size_t TURN_FACING_READS_UNKNOWN_SPRITE = 3;
constexpr Milliseconds TURN_FACING_REREAD(100);

//  The facing after a turn tap, read up to `reads` times.
std::optional<Direction> read_facing_after_tap(ConsoleHandle& console, ProControllerContext& context, PlayerGender player, size_t reads){
    std::optional<Direction> facing;
    for (size_t read = 1; read <= reads && !facing; read++){
        if (read > 1){
            pbf_wait(context, TURN_FACING_REREAD);
        }
        context.wait_for_all_requests();
        VideoSnapshot screen = console.video().snapshot();
        facing = screen ? read_player_facing(screen, player) : std::nullopt;
    }
    return facing;
}

//  Whether `at` is known and on its map's edge. Any unknown map or tile is
//  not.
bool known_on_map_edge(const std::optional<MapPosition>& at){
    if (!at){
        return false;
    }
    try{
        return on_map_edge(load_map(at->map), at->point);
    }catch (const Exception&){
        return false;
    }
}

//  After turn tap `tap`, the facing couldn't be read: taken as turned for
//  a sprite that isn't read or on a map's edge tile, else the leg fails.
void handle_unread_facing(
    ConsoleHandle& console, Direction direction, size_t tap,
    TurnSprite sprite, const std::optional<MapPosition>& at
){
    const std::string turn = std::format("after turn tap {} {}", tap, Pokemon::direction_name(direction));
    if (sprite == TurnSprite::NOT_READ){
        console.log(std::format("Route: {}, the facing can't be read (surfing or biking): taken as turned.", turn), COLOR_ORANGE);
        return;
    }
    if (known_on_map_edge(at)){
        console.log(std::format(
            "Route: {} on {}, facing not checked: on the map's edge tile, where a tile can hide the sprite.",
            turn, to_string(*at)
        ), COLOR_ORANGE);
        return;
    }
    //  Not taken as turned: nothing has settled, so the player's place is
    //  unknown. The leg fails, and the re-anchor finds it.
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format(
            "walk_overworld_route(): {}, the facing can't be read for {} reads: the player is not settled on screen.",
            turn, TURN_FACING_READS
        ),
        console
    );
}

} // namespace

void turn_to(
    ConsoleHandle& console, ProControllerContext& context, Direction direction, PlayerGender player,
    TurnSprite sprite, const std::optional<MapPosition>& at
){
    //  A tap while the player faces `direction` already takes a step, so the
    //  screen is read first. A trainer's battle leaves the player facing the
    //  way they stepped into the line, not the trainer (pokefirered
    //  trainer_see.c, TrainerSeeFunc_PrepareToEngage): 10.05's turn tap west
    //  after Emilia stepped, and "west 4 (open)" ended on Pokemon Tower 6F
    //  (4, 8), a tile long (console B, 2026-11-12 04:39).
    context.wait_for_all_requests();
    if (const VideoSnapshot screen = console.video().snapshot(); screen && read_player_facing(screen, player) == direction){
        console.log(std::format("Route: the player faces {} already: no turn tap.", Pokemon::direction_name(direction)));
        return;
    }
    const size_t reads = sprite == TurnSprite::WALKING ? TURN_FACING_READS : TURN_FACING_READS_UNKNOWN_SPRITE;
    for (size_t tap = 1; tap <= MAX_TURN_TAPS; tap++){
        pbf_press_dpad(context, dpad(direction), TURN_TAP, TURN_SETTLE);
        const std::optional<Direction> facing = read_facing_after_tap(console, context, player, reads);
        if (!facing){
            handle_unread_facing(console, direction, tap, sprite, at);
            return;
        }
        if (*facing == direction){
            return;
        }
        console.log(std::format(
            "Route: after turn tap {} {}, the player faces {}.",
            tap, Pokemon::direction_name(direction), Pokemon::direction_name(*facing)
        ), COLOR_ORANGE);
    }
}


} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
