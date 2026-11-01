/*  BDSP AutoStory - Pokemon Center
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_PokemonCenter_H
#define PokemonAutomation_PokemonBDSP_AutoStory_PokemonCenter_H

#include <optional>
#include <string>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_TileGrid.h"

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

//  Whether `screen` is the inside of a Pokemon Center seen from its door:
//  the orange back wall at the top left, and the black below the entrance
//  mat along the bottom edge. Its lit floor reads as a doorway
//  (find_lit_doorway()), so this is what says we are in (r126 runs 2, 3).
bool pokemon_center_inside(const ImageViewRGB32& screen);

//  Finds a lit doorway (a Pokemon Center's or a Poke Mart's) in the upper
//  two thirds of `screen`, the pale yellow-white of its open door. Nothing if
//  none shows there, or inside a Pokemon Center, whose floor is that colour.
std::optional<LitDoorway> find_lit_doorway(const ImageViewRGB32& screen);

//  Whether the screen shows a building's inside: its warm walls and floor
//  fill much of the view (Pokemon Centers, the Trainers' School: 42..65% of
//  the screen), and the room floats in black, which shows along at least one
//  edge (15% or more of a strip 12% deep). Warmth alone is not enough: the
//  evening tint makes outside 50..72% warm (r256, r257, Sandgem), and
//  Oreburgh's ground 58..69% by day. Of 1412 lab screenshots, the rule calls
//  every outside one outside, at all hours, and the rooms rooms. A fade is
//  dark everywhere but not warm, so it is not inside.
bool indoors(const ImageViewRGB32& screen);

//  After a wild battle out of doors: fails with a screenshot if the screen
//  turns into a room within 10 s, which means the party fainted and the
//  game took us home (r258 lost its third training battle in Route 202's
//  grass, woke in Twinleaf, and walked the Sandgem plan there). Returns
//  once the outside shows, not black and not a room.
void fail_if_blacked_out(VideoStream& stream, ProControllerContext& context, const std::string& label);

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
    const LitDoorway& doorway, const Pokemon::TileGrid& grid, const Pokemon::TilePos& door
);

//  For a player who walked to the row below the door on world tile `door`:
//  if its lit doorway shows further up the screen than straight ahead, the
//  walk ended short (someone in the way stops an open leg, and the walk
//  cannot tell), and the tiles he may stand on instead, read off the
//  doorway's offset, are returned to walk from again. Empty when he is at
//  the front row, or no doorway shows.
std::vector<Pokemon::TilePos> tiles_short_of_door(
    VideoStream& stream, ProControllerContext& context,
    const Pokemon::TileGrid& grid, const Pokemon::TilePos& door, const std::string& label
);

//  For a player on the row below a building's door, its lit doorway on
//  screen: steps across by the doorway's offset, looking again after each
//  step, until it is straight ahead. Does not go in, and does not turn north
//  to face it: a turn there walked us in (r127c). Returns false if no
//  doorway shows, a Center's inside shows, or after 6 steps.
bool line_up_with_lit_door(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);

//  Goes in through a building's door (a Pokemon Center's or a Poke Mart's).
//  Starts with its lit doorway on screen, within a few tiles: lines up with
//  the doorway by sight and walks in, looking again after every move. Whole-
//  tile steps sideways miss it, since movement is not locked to tiles, and
//  its fade may not read as black: a new scene also counts as inside. Fails
//  with a screenshot when no doorway shows, or after 12 moves.
void enter_lit_door(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);

//  Heals the party at a Pokemon Center. Starts in front of the Center with its
//  lit doorway on screen, within a few tiles of it: lines up with the doorway
//  by sight, walks in, talks to the nurse, accepts, and walks back out. Ends
//  on the tile below the door.
void heal_at_pokemon_center(
    VideoStream& stream, ProControllerContext& context,
    const std::string& label
);



}
}
}
#endif
