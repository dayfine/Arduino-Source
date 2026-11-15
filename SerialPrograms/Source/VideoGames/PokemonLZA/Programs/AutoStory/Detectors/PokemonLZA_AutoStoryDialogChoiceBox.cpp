/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryDialogChoiceBox.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


const ImageFloatBox& dialog_choice_box(){
    static const ImageFloatBox box{0.654308, 0.481553, 0.295529, 0.312621};
    return box;
}

const ImageFloatBox& dialog_choice_rows_box(){
    static const ImageFloatBox box{0.654308, 644.0 / 1080, 0.295529, 320.0 / 1080};
    return box;
}

const double DIALOG_CHOICE_ROW_SPACING = 24.0 / 1080;



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
