/*  PokemonLZA AutoStory - Readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  ScreenObservation::readings names PLZA's backend sets from its detectors.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryReadings_H
#define PokemonAutomation_PokemonLZA_AutoStoryReadings_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{



//  1: the autosave badge ("Saving...", top right).
constexpr std::string_view AUTOSAVE_READING = "scene.autosave";

//  1: the tutorial's controls hint ("(L) Move"): the player has control
//  (also on 01.01's station platform, which shows no party bar).
constexpr std::string_view CONTROLS_HINT_READING = "scene.controls_hint";

//  1: the advance chevron, at a dialogue box or boxless subtitles.
constexpr std::string_view ADVANCE_ARROW_READING = "scene.advance_arrow";

//  1: a main mission's banner ("MAIN MISSION 01" over a yellow bar).
constexpr std::string_view MISSION_BANNER_READING = "scene.mission_banner";

//  1: the "COMPLETE" card a main mission ends with; it waits for A. Excludes
//  MISSION_BANNER_READING.
constexpr std::string_view MISSION_COMPLETE_READING = "scene.mission_complete";

//  A choice box's row count; unset when the rows do not read.
constexpr std::string_view CHOICE_ROWS_READING = "choice.rows";

//  With CHOICE_ROWS_READING, the highlighted row, 0 the top.
constexpr std::string_view CHOICE_SELECTED_READING = "choice.selected";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
