/*  Player Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which way the player faces, read from the overworld. The camera keeps the
 *  player on the same spot of the screen, so the reader compares that spot
 *  with the player's standing sprites from pret/pokefirered, on the sprite's
 *  own pixels only, so the ground behind doesn't matter. The sprites come
 *  from Packages: Resources/PokemonFRLG/PlayerSprites/red_normal.png (the
 *  boy) and green_normal.png (the girl) are the decompilation's
 *  graphics/object_events/pics/people/ files of those names.
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


//  Who the player is, picked at "Are you a boy or a girl?" in the intro.
enum class PlayerGender{
    BOY,
    GIRL,
};

//  The standing player's frames (or another person's, match_facing_at()),
//  as the game draws them, with transparent pixels at alpha 0. Facing east
//  is facing west mirrored.
struct PlayerSprites{
    ImageRGB32 south;
    ImageRGB32 north;
    ImageRGB32 west;
};

//  Loads `gender`'s frames once. Throws FileException if the file is
//  missing.
const PlayerSprites& player_sprites(PlayerGender gender);


//  The way the standing player faces, or nothing if no standing sprite
//  matches clearly (surfing, on the bike, in a menu, mid-step). The boy's
//  sprites are tried, then the girl's, so a save of either player works
//  without being told which.
std::optional<Direction> read_player_facing(const ImageViewRGB32& screen);

//  The same on `game`, the GBA's 240x160 screen at any scale, already cut
//  out of the console's.
std::optional<Direction> match_player_facing(const ImageViewRGB32& game, const PlayerSprites& sprites);


//  The way a standing person faces whose `sprites` are drawn from row
//  `first_row` of their 16x32 frames, `dx_tiles` east and `dy_tiles` south
//  of the player, on `game` (as match_player_facing(), which is this at the
//  player's own tile). Nothing if no frame matches clearly.
std::optional<Direction> match_facing_at(const ImageViewRGB32& game, const PlayerSprites& sprites, int first_row, int dx_tiles, int dy_tiles);

//  Registers player facing reader unit tests.
void add_tests_PlayerFacingReader(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
