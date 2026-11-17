/*  BDSP AutoStory - Lit doorway
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A building's lit doorway and the street lamps seen on the overworld, and
 *  the tiles the player may stand on, read off them.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_LitDoorway_H
#define PokemonAutomation_PokemonBDSP_AutoStory_LitDoorway_H

#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Indoors.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A building's lit doorway seen on the overworld, relative to the player,
//  whom the camera keeps in the middle of the screen.
struct LitDoorway{
    double x;               //  centre across the screen, 0 to 1
    double y;               //  centre down the screen, 0 to 1
    double tiles_across;    //  east of the player if positive, west if negative
    double rows_up;         //  rows north of the player's tile, roughly: 1 straight ahead
};

//  Whether `screen` is the inside of a Pokemon Center: the orange back wall
//  along the top, and either the black below the entrance mat along the
//  bottom edge (seen from the door) or the nurse's counter close up. Its lit
//  floor reads as a doorway (find_lit_doorway()), so this is what says we
//  are in (r126 runs 2, 3).
bool pokemon_center_inside(const ImageViewRGB32& screen);

//  Finds a lit doorway (a Pokemon Center's or a Poke Mart's) in the upper
//  two thirds of `screen`, the pale yellow-white of its open door. Nothing if
//  none shows there, or inside a Pokemon Center, whose floor is that colour.
std::optional<LitDoorway> find_lit_doorway(const ImageViewRGB32& screen);

//  A lit street lamp's head inside `box` (0 to 1 of the screen): the mean
//  x of its white core, or nothing if fewer than a lamp's worth of white
//  pixels show there (by day the lamps are not lit).
std::optional<double> find_lit_lamp_x(const ImageViewRGB32& screen, const ImageFloatBox& box);

//  How many tiles a thing seen at screen x is across from the player
//  (positive: east).
double tiles_across_at(double screen_x);

//  Moves across by `tiles`, east if positive, west if negative, facing that
//  way first.
void move_across(VideoStream& stream, ProControllerContext& context, double tiles);

//  The tiles a player who sees `doorway` (the door on world tile `door`) may
//  stand on: the one its offset says, give or take one each way, since the
//  rows are a rough count. Only standable tiles of `grid`.
std::vector<Pokemon::TilePos> tiles_by_lit_doorway(
    const LitDoorway& doorway, const StoryWorld& grid, const Pokemon::TilePos& door
);

//  For a player who walked to the row below the door on world tile `door`:
//  if its lit doorway shows further up the screen than straight ahead, the
//  walk ended short (someone in the way stops an open leg, and the walk
//  cannot tell), and the tiles he may stand on instead, read off the
//  doorway's offset, are returned to walk from again. Empty when he is at
//  the front row, or no doorway shows.
std::vector<Pokemon::TilePos> tiles_short_of_door(
    VideoStream& stream, ProControllerContext& context,
    const StoryWorld& grid, const Pokemon::TilePos& door, const std::string& label
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
