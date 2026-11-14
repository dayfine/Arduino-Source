/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a dialogue's choices are drawn, for the selection arrow's
 *  detector: the screen events' choice box and choose_dialog_option()
 *  (Actions/PokemonLZA_AutoStoryDialogs.h) both read it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_H

#include "CommonFramework/ImageTools/ImageBoxes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Where a dialogue's choices are drawn: the restaurant receptionist's
//  (RestaurantFarmer). Other dialogues' choices are assumed to sit there.
const ImageFloatBox& dialog_choice_box();



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
