/*  AutoStory Gender Menu
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/Images/SolidColorTest.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_SelectionArrowDetector.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_AutoStoryGenderMenu.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<int> gender_menu_cursor(const ImageViewRGB32& screen){
    //  On the 240x160 screen: the menu's window spans x 144-216, y 72-104,
    //  and this box (x 190-212, y 76-100) is white there, right of BOY and
    //  GIRL. The intro's message box is lower down.
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    if (!is_white(extract_box_reference(game, ImageFloatBox(190.0 / 240, 76.0 / 160, 22.0 / 240, 24.0 / 160)))){
        return std::nullopt;
    }
    for (int row = 0; row < 2; row++){
        //  rival_name_list_cursor()'s arrow box, moved to the menu's cursor
        //  at x 144, y 73 + 16 * row.
        const ImageFloatBox box(142.6 / 240, (73.4 + 16 * row) / 160, 0.0369231, 0.0778846);
        if (SelectionArrowDetector(COLOR_RED, nullptr, box).detect(screen)){
            return row;
        }
    }
    return std::nullopt;
}

GenderMenuStep gender_menu_step(std::optional<int> cursor_row, PlayerGender wanted){
    if (!cursor_row || wanted == PlayerGender::BOY){
        return GenderMenuStep::NONE;
    }
    const int target = 1;
    if (*cursor_row < target){
        return GenderMenuStep::DOWN;
    }
    if (*cursor_row > target){
        return GenderMenuStep::UP;
    }
    return GenderMenuStep::CONFIRM;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
