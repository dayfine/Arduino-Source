/*  Boulder Push Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageDiff.h"
#include "PokemonFRLG_OverworldRecoveryWalk.h"
#include "PokemonFRLG_BoulderPushReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr double GBA_WIDTH = 240;
constexpr double GBA_HEIGHT = 160;
constexpr double TILE = 16;
//  The player's tile on the GBA screen while the camera follows them: column
//  7, row 4 of the 15 by 10 (PlayerFacingReader's sprite stands on it).
constexpr double PLAYER_X = 112;
constexpr double PLAYER_Y = 72;

//  How much the two tiles ahead change when a boulder moves a tile on: 76.5
//  for the boulder pushed south from Victory Road 1F's (7, 17) (console B,
//  2026-11-16, 19.04b), against 11.3 between two of the cave's floor tiles.
constexpr double BOULDER_MOVED_RMSD = 30.0;

//  The two tiles ahead of the player, `direction`, as a box of `game`: from
//  the tile next to theirs to the one after it.
ImageFloatBox tiles_ahead(const ImageFloatBox& game, Direction direction){
    const TilePos next = TilePos{0, 0}.next(direction);
    const bool across = next.x != 0;
    const double x = PLAYER_X + (next.x < 0 ? 2 * next.x : next.x) * TILE;
    const double y = PLAYER_Y + (next.y < 0 ? 2 * next.y : next.y) * TILE;
    const double width = across ? 2 * TILE : TILE;
    const double height = across ? TILE : 2 * TILE;
    return ImageFloatBox(
        game.x + game.width * x / GBA_WIDTH,
        game.y + game.height * y / GBA_HEIGHT,
        game.width * width / GBA_WIDTH,
        game.height * height / GBA_HEIGHT
    );
}

}  // namespace


BoulderPressRead read_boulder_press(
    const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game, Direction direction
){
    BoulderPressRead read;
    read.scrolled = tap_scroll_change(before, after, game) > WALL_TAP_MOVED_RMSD;
    if (!read.scrolled){
        const ImageFloatBox ahead = tiles_ahead(game, direction);
        read.boulder_moved = ImageMatch::pixel_RMSD(
            extract_box_reference(before, ahead), extract_box_reference(after, ahead)
        ) > BOULDER_MOVED_RMSD;
    }
    return read;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
