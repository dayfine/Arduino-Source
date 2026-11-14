/*  PokemonLZA AutoStory - Readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The names of the readings (ScreenObservation::readings) PLZA's backend
 *  sets from its AutoStory detectors, which its scene states and step
 *  checks match on.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryReadings_H
#define PokemonAutomation_PokemonLZA_AutoStoryReadings_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The reading (ScreenObservation::readings) set to 1 when the station
//  platform shows with free control after the opening cutscene
//  (PlatformArrivalDetector).
constexpr std::string_view PLATFORM_READING = "scene.platform";
//  The reading set to 1 when the cutscene's "Hold button to skip" badge
//  shows (SkipPromptDetector).
constexpr std::string_view SKIP_PROMPT_READING = "scene.skip_prompt";
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
//  over a yellow bar with the mission's name) shows (MainMissionBannerDetector).
constexpr std::string_view MISSION_BANNER_READING = "scene.mission_banner";
//  The reading set to 1 while the card a main mission ends with ("COMPLETE"
//  over the mission's yellow bar) shows; it waits for A
//  (MissionCompleteDetector). A look with it has no MISSION_BANNER_READING.
constexpr std::string_view MISSION_COMPLETE_READING = "scene.mission_complete";
//  The reading set to 1 while a tutorial card (or a splash) shows; only A
//  turns its pages, B does not close it (tutorial_card_shows(),
//  Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h).
constexpr std::string_view TUTORIAL_CARD_READING = "scene.tutorial_card";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
