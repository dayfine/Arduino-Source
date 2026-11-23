/*  Boulder Push Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What a step tap toward a Strength boulder did, read from the game screen
 *  before and after it. A tap into a boulder pushes it a tile while the
 *  player walks in place (pret/pokefirered field_player_avatar.c,
 *  DoBoulderDust: the player's WalkInPlaceNormal, the boulder's
 *  WalkSlower): the map stays, and only the two tiles ahead of the player
 *  change. The next tap walks the player onto the tile the boulder left,
 *  and the map scrolls a tile.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BoulderPushReader_H
#define PokemonAutomation_PokemonFRLG_BoulderPushReader_H

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What one step tap toward a boulder showed.
struct BoulderPressRead{
    //  The map scrolled a tile in the tap's direction: the player walked a
    //  tile (edge bands changed over WALL_TAP_MOVED_RMSD and the map above
    //  the dialog box shifted one tile that way). A dialog box opening or
    //  closing on a still screen, or a wild battle flash, reads as neither
    //  scrolled nor boulder_moved so press_for() clears the interrupt.
    bool scrolled = false;
    //  The two tiles ahead of the player changed while the edge bands stayed
    //  still: a boulder left the first for the second, or for a hole there.
    bool boulder_moved = false;
};

//  Reads a tap `direction` from `before` and `after`, shots of the console
//  screen with the GBA's 240x160 screen in `game`.
BoulderPressRead read_boulder_press(
    const ImageViewRGB32& before, const ImageViewRGB32& after, const ImageFloatBox& game, Direction direction
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
