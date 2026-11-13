/*  Pokemon Center Floor Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether the overworld shows a Pokemon Center's ground floor, by its cream
 *  floor tiles. Every Center's ground floor but the Indigo Plateau's and One
 *  Island's has the one layout (PokemonFRLG_PokemonCenterRoom.h), and the
 *  floor fills much of the screen wherever the player stands in it. Water,
 *  sand bars, streets and grass have none of it: Route 21's sand is a
 *  darker yellow (about 248, 233, 120 against the floor's 246, 242, 167).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_PokemonCenterFloorReader_H
#define PokemonAutomation_PokemonFRLG_PokemonCenterFloorReader_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The share of `game`'s pixels (the GBA screen at any scale, already cut
//  out of the console's) that are a Center's floor colour. 0 for an empty
//  image.
double pokemon_center_floor_share(const ImageViewRGB32& game);

//  Whether `screen`, the console's frame, shows a Center's ground floor.
//  False for an empty frame.
bool shows_pokemon_center_floor(const ImageViewRGB32& screen);


//  Registers the reader's unit tests.
void add_tests_PokemonCenterFloorReader(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
