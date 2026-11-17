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
#include <string_view>
#include <vector>
#include "CommonFramework/Language.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  From the overworld: opens Options and sets what the story needs (fast
//  text, Send to Boxes automatic, no nickname prompt, Autosave off), saves
//  them and returns to the overworld. Tries four times, backing out to the
//  overworld between tries, and fails with a screenshot before any movement.
void verify_tutorial_settings(VideoStream& stream, ProControllerContext& context);


namespace detail{

//  One row of the Options list, as the story sets it.
struct OptionsRow{
    //  The reference images' name: <label>_label.png, <label>_value.png.
    std::string_view label;
    //  The value the story picks, for the log; empty when the story leaves
    //  the row as it is.
    std::string_view value;
    //  How many values the row shows side by side, and which one to pick:
    //  the check when the language has no label images. No column: a
    //  slider (checked by its number) or the Change Window arrow picker.
    size_t columns = 0;
    std::optional<size_t> column = std::nullopt;
    bool slider = false;
};
//  The Options list from the top, and whether its rows have label images.
struct OptionsLayout{
    std::vector<OptionsRow> rows;
    bool label_images = false;
};
//  The Options layout for a game language. Label images are English only,
//  so far. Japanese has a Kana/Kanji row below Text Speed; the other
//  languages have the English rows.
OptionsLayout options_layout(Language language);
//  Which of `columns` values side by side holds red ink centred at `x` (a
//  fraction of the screen width), or none when `x` is outside the values.
std::optional<size_t> options_value_column(double x, size_t columns);

} // namespace detail



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
