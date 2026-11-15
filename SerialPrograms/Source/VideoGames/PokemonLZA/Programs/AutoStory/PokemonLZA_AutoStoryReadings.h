/*  PokemonLZA AutoStory - Readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The names of the readings (ScreenObservation::readings) PLZA's backend
 *  sets from its AutoStory detectors, which its steps match on.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryReadings_H
#define PokemonAutomation_PokemonLZA_AutoStoryReadings_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The reading set to 1 when the station platform shows with free control
//  after the opening cutscene (PlatformArrivalDetector).
constexpr std::string_view PLATFORM_READING = "scene.platform";

//  The reading set to 1 while the autosave badge (a pencil and "Saving...",
//  top right) shows: the game has autosaved (AutosaveDetector).
constexpr std::string_view AUTOSAVE_READING = "scene.autosave";

//  The reading set to 1 while the tutorial's controls hint (a "?" card over
//  "(L) Move" and "(R) Control Camera", under the player) shows: the player
//  has control (ControlsHintDetector).
constexpr std::string_view CONTROLS_HINT_READING = "scene.controls_hint";

//  The reading set to 1 while the "press A to go on" chevron shows, at a
//  dialogue box or at boxless subtitles (AdvanceArrowDetector).
constexpr std::string_view ADVANCE_ARROW_READING = "scene.advance_arrow";

//  The reading set to 1 while a main mission's banner ("MAIN MISSION 01"
//  over a yellow bar with the mission's name) shows
//  (MainMissionBannerDetector).
constexpr std::string_view MISSION_BANNER_READING = "scene.mission_banner";

//  With a choice box on screen (ScreenEvent::CHOICE_BOX), how many rows
//  the menu has (read_dialog_choice_rows()); unset when they do not read.
constexpr std::string_view CHOICE_ROWS_READING = "choice.rows";

//  With CHOICE_ROWS_READING, the highlighted row, 0 the top.
constexpr std::string_view CHOICE_SELECTED_READING = "choice.selected";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
