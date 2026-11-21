/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Pure platform wait and text-speed setup utilities operating through PlzaBackend.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Passively waits (no button presses) until the station platform is live with free control.
//  Fails as "platform_not_seen" after `timeout`, or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout);

//  Opens the main menu options, sets text speed to Fast, and returns to the overworld.
//  Fails as "text_speed_timeout" on timeout or "game_not_running" on HOME.
std::optional<Pokemon::StoryFailure> set_text_speed_fast(PlzaBackend& backend);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
