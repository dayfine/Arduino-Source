/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a dialogue's choices are drawn, for the selection arrow's
 *  detector: the screen events' choice box and choose_dialog_option()
 *  (Actions/PokemonLZA_AutoStoryDialogs.h) both read it, and the choice
 *  rows as get_current_selector_index()
 *  (Programs/PokemonLZA_FastTravelNavigation.h) counts them.
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

//  The choice rows for get_current_selector_index(): its top is half a row
//  gap above the top row's arrow, four rows tall. Measured on the tourist
//  choice (two rows; on 1920x1080 the panel's top at y 640, the arrow
//  y 656-712, the rows 80 apart). Other choices are assumed to put their
//  top row there.
const ImageFloatBox& dialog_choice_rows_box();

//  The gap between one choice row's arrow and the next, as
//  get_current_selector_index() takes it: the row pitch less the arrow's
//  height (80 - 56 of 1080).
extern const double DIALOG_CHOICE_ROW_SPACING;



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
