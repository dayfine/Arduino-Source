/*  PokemonLZA AutoStory - Platform wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The station platform, where the opening ends (01.01's start).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPlatformWait_H
#define PokemonAutomation_PokemonLZA_AutoStoryPlatformWait_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Passively waits (no button presses) until the station platform is live with
//  free control (controls hint up, no autosave badge or dialog box).
//  Fails as "platform_not_seen" after `timeout`, or GAME_NOT_RUNNING on HOME.
std::optional<Pokemon::StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout);

//  Precondition: the new-game opening sequence has just been confirmed.
//  Advances and skips the opening until the arrival autosave appears, then
//  waits passively for the platform's free-control screen within the shared
//  `timeout`. Fails as "platform_not_seen" or GAME_NOT_RUNNING on HOME.
std::optional<Pokemon::StoryFailure> opening_to_platform(PlzaBackend& backend, Milliseconds timeout);




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
