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

//  Finds a lit doorway (a Pokemon Center's or a Poke Mart's) in the upper
//  two thirds of `screen`, the pale yellow-white of its open door. Nothing if
//  none shows there.
std::optional<LitDoorway> find_lit_doorway(const ImageViewRGB32& screen);

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
