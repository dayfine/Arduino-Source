/*  PokemonLZA AutoStory - Guarded walks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The checks around a story walk (PokemonLZA_AutoStoryMoves.h), which
 *  itself reads nothing: it starts only on a screen with no box, choice,
 *  fade or battle, and a walk that ends on a fade stops on it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryGuardedWalks_H
#define PokemonAutomation_PokemonLZA_AutoStoryGuardedWalks_H

#include <string_view>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEventTypes.h"
#include "PokemonLZA_AutoStoryMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Throws OperationFailedException unless the screen shows no box, choice,
//  fade or battle on two reads: a walk started anywhere else walks blind.
//  The tutorial has no party bar, so it is not required.
void check_can_walk(ConsoleHandle& console, ProControllerContext& context, std::string_view walk);

//  Walks `move`, holding its last leg until `end` shows (a fade), the leg's
//  time as the upper bound. Throws OperationFailedException if `end` does
//  not show within it.
void walk_until(
    ConsoleHandle& console, ProControllerContext& context,
    std::string_view walk, const StoryMove& move, Pokemon::ScreenEvent end
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
