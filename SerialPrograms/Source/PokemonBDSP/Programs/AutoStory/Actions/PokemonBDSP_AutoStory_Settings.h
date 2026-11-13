/*  BDSP AutoStory - Settings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Settings_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Settings_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The Options screen, read from its layout and colours only, so that it
//  reads the same in every game language.

//  Whether the frame shows Options: the blue heading bar over the white panel.
bool options_screen_shown(const ImageViewRGB32& screen);

//  The centre of the selected row (its red outline), as a fraction of the
//  screen height; none without exactly one outlined row.
std::optional<double> options_selected_row_y(const ImageViewRGB32& screen);

//  Which of the `columns` value columns (2 or 3) of the row centred at `y`
//  holds the highlighted (red) value, counted from the left; none when no
//  column or more than one is highlighted.
std::optional<size_t> options_highlighted_column(const ImageViewRGB32& screen, double y, size_t columns);


//  From the overworld: opens Options and sets what the story needs (fast
//  text, Send to Boxes automatic, no nickname prompt, Autosave off), saves
//  them and returns to the overworld. Tries four times, backing out to the
//  overworld between tries, and fails with a screenshot before any movement.
void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
