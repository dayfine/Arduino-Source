/*  PokemonLZA AutoStory - Platform wait
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The station platform, where the opening ends (01.01's start).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPlatformWait_H
#define PokemonAutomation_PokemonLZA_AutoStoryPlatformWait_H

#include <cstddef>
#include <optional>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


enum class PlatformWait{
    PLATFORM_ONLY,      //  "wait_for_platform": a start check
    AFTER_AUTOSAVE,     //  "opening_to_platform": the autosave badge first
};

//  One A per text box with the advance arrow (A does not advance the
//  cutscene). AFTER_AUTOSAVE: Plus held on the opening's first moving look,
//  which skips it to the platform. Fails as "platform_not_seen".
std::optional<Pokemon::StoryFailure> wait_for_platform(PlzaBackend& backend, Milliseconds timeout, PlatformWait wait);

//  Whether the platform shows on two looks in a row within `looks` looks.
bool platform_holds_within(PlzaBackend& backend, size_t looks);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
