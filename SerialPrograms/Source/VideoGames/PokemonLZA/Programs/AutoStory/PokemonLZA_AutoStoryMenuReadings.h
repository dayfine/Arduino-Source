/*  PokemonLZA AutoStory - Menu readings
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Readings from the main menu's detectors, for the menu save, and the map's.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMenuReadings_H
#define PokemonAutomation_PokemonLZA_AutoStoryMenuReadings_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  1 while the main menu (X) shows.
constexpr std::string_view MAIN_MENU_READING = "menu.main";

//  1 while the arrow is on Save, after R on the main menu.
constexpr std::string_view MENU_SAVE_BUTTON_READING = "menu.save_button";

//  1: the full map, which Plus opens on free control.
constexpr std::string_view MAP_READING = "menu.map";



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
