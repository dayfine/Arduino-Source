/*  Player Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which way the player faces, read from the overworld. The camera keeps the
 *  player on the same spot of the screen, so the reader compares that spot
 *  with the player's standing sprites from pret/pokefirered, on the sprite's
 *  own pixels only, so the ground behind doesn't matter.
 *
 *  Only the walking player is known: surfing and biking use other sprites.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_PlayerFacingReader_H
#define PokemonAutomation_PokemonFRLG_PlayerFacingReader_H

#include <optional>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
    class ImageViewRGB32;
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The way the standing player faces, or nothing if no standing sprite
//  matches clearly (surfing, on the bike, in a menu, mid-step).
std::optional<MapDirection> read_player_facing(const ImageViewRGB32& screen);


void add_tests_PlayerFacingReader(UnitTestDatabase& database);



}
}
}
#endif
