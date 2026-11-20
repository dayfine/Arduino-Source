/*  PokemonLZA AutoStory - Menu save
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure backend actions for saving via the main menu and setting text speed to Fast.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMenuSave_H
#define PokemonAutomation_PokemonLZA_AutoStoryMenuSave_H

#include <optional>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Opens the main menu, triggers a manual save, and returns to the overworld.
//  Fails as "menu_save_timeout" on timeout or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> save_from_menu(PlzaBackend& backend);

//  Opens the main menu options, sets text speed to Fast, and returns to the overworld.
//  Fails as "text_speed_timeout" on timeout or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> set_text_speed_fast(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
