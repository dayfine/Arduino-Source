/*  Trainer Facing Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which way a trainer standing near the player faces, read from the
 *  overworld with match_facing_at() against their standing frames. The
 *  frames are pret/pokefirered's: a rocker (OBJ_EVENT_GFX_ROCKER, Juggler
 *  Kayden in Fuchsia Gym) from graphics/object_events/pics/people/rocker.png
 *  in the colours the game gives it, graphics/object_events/palettes/
 *  npc_blue.pal (gObjectEventGraphicsInfo_Rocker: OBJ_EVENT_PAL_TAG_NPC_BLUE).
 *  They are kept here, not in Resources, until a Packages change carries
 *  the sheet.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_TrainerFacingReader_H
#define PokemonAutomation_PokemonFRLG_TrainerFacingReader_H

#include <optional>
#include <string_view>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The standing frames of the trainer sprite `sprite` ("rocker"), or null
//  for one this reader hasn't got.
const PlayerSprites* trainer_sprites(std::string_view sprite);

//  The way the trainer drawn with `sprites`, standing `dx_tiles` east and
//  `dy_tiles` south of the player, faces on the console's `screen`; nothing
//  if no frame matches clearly (another person there, mid-turn, a menu).
std::optional<Direction> read_trainer_facing(
    const ImageViewRGB32& screen, const PlayerSprites& sprites, int dx_tiles, int dy_tiles
);

//  Registers the trainer facing reader's tests.
void add_tests_TrainerFacingReader(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
