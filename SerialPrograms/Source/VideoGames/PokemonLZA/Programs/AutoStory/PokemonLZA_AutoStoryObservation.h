/*  PokemonLZA AutoStory - Observations
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's screen events (PokemonLZA_AutoStoryScreenEvents.h) as the story
 *  engine's observations, which its backend returns and its scene states
 *  match on.
 *
 */

// code-health: allow header_symbols -- one reading name per detector; the readings are one concern that every consumer shares
#ifndef PokemonAutomation_PokemonLZA_AutoStoryObservation_H
#define PokemonAutomation_PokemonLZA_AutoStoryObservation_H

#include <optional>
#include <string_view>
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEventTypes.h"

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

//  PlzaScreenObserver's events as one look for the story engine:
//  DIALOG_BOX is TEXT_BOX; CHOICE_BOX adds TEXT_BOX;
//  BATTLE_STARTED is BATTLE; PLAYER_CAN_MOVE (the party bar) is CAN_MOVE;
//  SCREEN_STILL is STILL; the fades and INTERACT_PROMPT keep their names.
//  No time and no frame: the backend adds them.
ScreenObservation plza_observation(const Pokemon::ScreenEvents& events);

//  The engine's event for a fade of the shared events; none for another
//  event.
std::optional<PokemonAutomation::ScreenEvent> engine_fade_event(Pokemon::ScreenEvent event);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
