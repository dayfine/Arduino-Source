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

//  How closely the RMSD over the map shifted one tile must beat the unshifted
//  RMSD for an edge-band change to read as a step rather than a dialog box or
//  battle flash (same ratio as tap_moved_on_water()).
constexpr double SCROLL_STEP_RATIO = 0.8;
//  Rows 0..6 of the 10-row GBA screen (y = 0..112 of 160), clear of the
//  white dialog box in rows 7..9 (y = 116..158): a step that also opens
//  "REPEL's effect wore off..." still scrolls these 7 rows by one tile,
//  while a tap that only opens or closes that box leaves them still.
constexpr size_t ROWS_ABOVE_DIALOG = 7;

//  Whether the upper ROWS_ABOVE_DIALOG rows of `game` in `after` match
//  `before` shifted one tile in `direction` more closely than `before`
//  unshifted.
bool scrolled_above_dialog(
    const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game, Direction direction
){
    if (before.width() != after.width() || before.height() != after.height()){
        return false;
    }
    const size_t game_x = (size_t)(before.width() * game.x + 0.5);
    const size_t game_y = (size_t)(before.height() * game.y + 0.5);
    const size_t game_width = (size_t)(before.width() * game.width + 0.5);
    const size_t game_height = (size_t)(before.height() * game.height + 0.5);
    const size_t tile_width = game_width / 15;
    const size_t tile_height = game_height / 10;
    const size_t above_height = tile_height * ROWS_ABOVE_DIALOG;
    if (tile_width == 0 || tile_height == 0
        || game_x + game_width > before.width()
        || game_y + above_height > before.height()
    ){
        return false;
    }
    size_t x = game_x;
    size_t y = game_y;
    size_t from_x = game_x;
    size_t from_y = game_y;
    size_t width = game_width;
    size_t height = above_height;
    switch (direction){
    case Direction::NORTH:
        y += tile_height;
        height -= tile_height;
        break;
    case Direction::SOUTH:
        from_y += tile_height;
        height -= tile_height;
        break;
    case Direction::WEST:
        x += tile_width;
        width -= tile_width;
        break;
    case Direction::EAST:
        from_x += tile_width;
        width -= tile_width;
        break;
    }
    const ImageViewRGB32 now = after.sub_image(x, y, width, height);
    const double still = ImageMatch::pixel_RMSD(before.sub_image(x, y, width, height), now);
    const double stepped = ImageMatch::pixel_RMSD(before.sub_image(from_x, from_y, width, height), now);
    return stepped < still * SCROLL_STEP_RATIO;
}

}  // namespace


BoulderPressRead read_boulder_press(
    const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game, Direction direction
){
    BoulderPressRead read;
    if (tap_scroll_change(before, after, game) > WALL_TAP_MOVED_RMSD){
        read.scrolled = scrolled_above_dialog(before, after, game, direction);
        return read;
    }
    const ImageFloatBox ahead = tiles_ahead(game, direction);
    read.boulder_moved = ImageMatch::pixel_RMSD(
        extract_box_reference(before, ahead), extract_box_reference(after, ahead)
    ) > BOULDER_MOVED_RMSD;
    return read;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
