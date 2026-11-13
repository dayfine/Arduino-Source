/*  Overworld Facing
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonFRLG/Inference/Dialogs/PokemonFRLG_DialogDetector.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_StartMenuDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
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

TurnTaps tap_until_facing(
    Direction direction, size_t reads,
    const std::function<void()>& tap,
    const std::function<std::optional<Direction>(size_t read)>& read
){
    TurnTaps out;
    while (out.taps < MAX_TURN_TAPS){
        tap();
        out.taps++;
        out.facing.reset();
        for (size_t index = 1; index <= reads && !out.facing; index++){
            out.facing = read(index);
        }
        if (!out.facing || *out.facing == direction){
            return out;
        }
    }
    return out;
}

bool unread_turn_taken(const UnreadTurn& turn){
    return turn.sprite == TurnSprite::NOT_READ || (turn.at_known && turn.overworld_clear);
}

namespace{

//  Surfing and biking draw other sprites, which read_player_facing() doesn't
//  know: a few reads, then the old way, taken as turned.
constexpr size_t TURN_FACING_READS_UNKNOWN_SPRITE = 3;
constexpr Milliseconds TURN_FACING_REREAD(100);

//  Whether `screen` shows the overworld with nothing over it: no START
//  menu, menu or message, and not the black of a fade.
bool overworld_clear(const VideoSnapshot& screen){
    if (!screen){
        return false;
    }
    if (is_black(extract_box_reference(screen, GameSettings::instance().GAME_BOX))){
        return false;
    }
    return !StartMenuDetector(COLOR_RED).detect(screen)
        && !SelectionDialogDetector(COLOR_RED).detect(screen)
        && !WhiteDialogDetector(COLOR_RED).detect(screen);
}

//  Whether `at` is known and on its map's edge, on the maps the routes
//  are planned on (overworld_maps(), with the coded Center rooms). Any
//  unknown map or tile is not.
bool known_on_map_edge(const std::optional<MapPosition>& at){
    if (!at){
        return false;
    }
    try{
        const std::vector<MapData> maps = overworld_maps();
        const auto map = std::find_if(maps.begin(), maps.end(), [&](const MapData& item){ return item.name == at->map; });
        return map != maps.end() && on_map_edge(*map, at->point);
    }catch (const Exception&){
        return false;
    }
}

//  After turn tap `tap`, the facing couldn't be read: taken as turned as
//  unread_turn_taken() says, else the leg fails.
void handle_unread_facing(
    ConsoleHandle& console, ProControllerContext& context, Direction direction, size_t tap,
    TurnSprite sprite, const std::optional<MapPosition>& at
){
    const std::string turn = std::format("after turn tap {} {}", tap, Pokemon::direction_name(direction));
    if (sprite == TurnSprite::NOT_READ){
        console.log(std::format("Route: {}, the facing can't be read (surfing or biking): taken as turned.", turn), COLOR_ORANGE);
        return;
    }
    context.wait_for_all_requests();
    const VideoSnapshot screen = console.video().snapshot();
    const bool clear = overworld_clear(screen);
    if (unread_turn_taken({.sprite = sprite, .at_known = at.has_value(), .overworld_clear = clear})){
        console.log(std::format(
            "Route: {} on {}, facing not read{}: taken as turned, the leg's walk checks it.",
            turn, to_string(*at),
            known_on_map_edge(at) ? " (the map's edge tile, where a tile can hide the sprite)" : " (a roof, a tree or a wanderer drawn over the sprite)"
        ), COLOR_ORANGE);
        return;
    }
    //  Not taken as turned: the route doesn't know where the player is, or
    //  something is over the overworld (a fade, a box, a scene). The leg
    //  fails, and the re-anchor finds the player.
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        std::format(
            "walk_overworld_route(): {}, the facing can't be read for {} reads: {}.",
            turn, TURN_FACING_READS, at ? "the overworld isn't clear" : "the player's tile isn't known"
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
    size_t taps = 0;
    std::optional<Direction> last;
    const TurnTaps turned = tap_until_facing(
        direction, reads,
        [&]{
            if (taps > 0 && last){
                console.log(std::format(
                    "Route: after turn tap {} {}, the player faces {}.",
                    taps, Pokemon::direction_name(direction), Pokemon::direction_name(*last)
                ), COLOR_ORANGE);
            }
            taps++;
            pbf_press_dpad(context, dpad(direction), TURN_TAP, TURN_SETTLE);
        },
        [&](size_t read){
            if (read > 1){
                pbf_wait(context, TURN_FACING_REREAD);
            }
            context.wait_for_all_requests();
            const VideoSnapshot screen = console.video().snapshot();
            last = screen ? read_player_facing(screen, player) : std::nullopt;
            return last;
        }
    );
    if (!turned.facing){
        handle_unread_facing(console, context, direction, turned.taps, sprite, at);
    }
}


} // namespace OverworldWalkSteps
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
