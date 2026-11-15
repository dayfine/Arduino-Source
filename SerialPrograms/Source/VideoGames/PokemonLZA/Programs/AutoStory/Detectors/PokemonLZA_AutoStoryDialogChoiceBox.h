/*  PokemonLZA AutoStory - Dialog choice box
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where a dialogue's choices are drawn, for the selection arrow's
 *  detector (the screen events' choice box and choose_dialog_option() in
 *  Actions/PokemonLZA_AutoStoryDialogs.h), and a reader for the choice
 *  rows: how many there are and which one is highlighted.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogChoiceBox_H

#include <cstddef>
#include <optional>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Where a dialogue's choices are drawn: the restaurant receptionist's
//  (RestaurantFarmer). Other dialogues' choices are assumed to sit there.
const ImageFloatBox& dialog_choice_box();

//  A dialogue's choice menu as read: how many rows it has, and the
//  highlighted one, 0 the top.
struct DialogChoiceRows{
    size_t count = 0;
    size_t selected = 0;
};

//  Reads the choice menu on `frame`. The menu's panel is bottom-anchored:
//  its last row sits at the same place whatever the row count, and each
//  further row adds one row pitch on top. So the rows are counted from the
//  bottom up, each either highlighted (white) or plain (dark blue, its
//  text's start bright on it), until a slot is neither; the selected row is
//  then counted from the top. None if no row, or not exactly one, is
//  highlighted.
std::optional<DialogChoiceRows> read_dialog_choice_rows(const ImageViewRGB32& frame);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
