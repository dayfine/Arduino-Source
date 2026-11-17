/*  PokemonLZA AutoStory - Menu readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The names of the readings (ScreenObservation::readings) PLZA's backend
 *  sets from the main menu's detectors, which the menu save matches on.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMenuReadings_H
#define PokemonAutomation_PokemonLZA_AutoStoryMenuReadings_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The reading set to 1 while the main menu (X) shows (MainMenuDetector).
constexpr std::string_view MAIN_MENU_READING = "menu.main";

//  The reading set to 1 while the selection arrow shows where
//  save_game_to_menu() reads the save's Save, after R on the main menu.
constexpr std::string_view MENU_SAVE_BUTTON_READING = "menu.save_button";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
