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

//  The reading set to 1 while the "press A to go on" chevron shows, at a
//  dialogue box or at boxless subtitles (AdvanceArrowDetector).
constexpr std::string_view ADVANCE_ARROW_READING = "scene.advance_arrow";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
