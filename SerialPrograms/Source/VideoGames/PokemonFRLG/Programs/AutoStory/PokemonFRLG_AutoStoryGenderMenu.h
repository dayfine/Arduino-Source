/*  AutoStory Gender Menu
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game intro's "Are you a boy or a girl?" menu.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGenderMenu_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGenderMenu_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PlayerFacingReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The arrow's row on the BOY/GIRL menu (0 BOY, 1 GIRL), if it shows. In
//  pokefirered's oak_speech.c the menu is a 9x4-tile window at tile
//  (18, 9), rows 16 px apart, the cursor starting on BOY.
std::optional<int> gender_menu_cursor(const ImageViewRGB32& screen);

//  What the intro does on the BOY/GIRL menu. The boy is left to the A every
//  1.5 s that advances the intro, as before; the girl is moved to and
//  picked.
enum class GenderMenuStep{
    NONE,
    DOWN,
    UP,
    CONFIRM,
};
//  The step for the arrow on row `cursor_row` (0 BOY, 1 GIRL), or none if
//  the menu doesn't show.
GenderMenuStep gender_menu_step(std::optional<int> cursor_row, PlayerGender wanted);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
