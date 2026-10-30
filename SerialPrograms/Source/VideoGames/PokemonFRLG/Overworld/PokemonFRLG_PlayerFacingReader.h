/*  Player Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which way the player faces, read from the overworld. The camera keeps the
 *  player on the same spot of the screen, so the reader compares that spot
 *  with the player's standing sprites from pret/pokefirered, on the sprite's
 *  own pixels only, so the ground behind doesn't matter. The sprites come
 *  from Packages: Resources/PokemonFRLG/PlayerSprites/red_normal.png is the
 *  decompilation's graphics/object_events/pics/people/red_normal.png.
 *
 *  Only the walking player is known: surfing and biking use other sprites.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_PlayerFacingReader_H
#define PokemonAutomation_PokemonFRLG_PlayerFacingReader_H

#include <optional>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonFRLG_OverworldMap.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The standing player's frames, as the game draws them, with transparent
//  pixels at alpha 0. Facing east is facing west mirrored.
struct PlayerSprites{
    ImageRGB32 south;
    ImageRGB32 north;
    ImageRGB32 west;
};

//  Loads the frames once. Throws FileException if the file is missing.
const PlayerSprites& player_sprites();


//  The way the standing player faces, or nothing if no standing sprite
//  matches clearly (surfing, on the bike, in a menu, mid-step).
std::optional<Direction> read_player_facing(const ImageViewRGB32& screen);

//  The same on `game`, the GBA's 240x160 screen at any scale, already cut
//  out of the console's.
std::optional<Direction> match_player_facing(const ImageViewRGB32& game, const PlayerSprites& sprites);


void add_tests_PlayerFacingReader(UnitTestDatabase& database);



}
}
}
#endif
