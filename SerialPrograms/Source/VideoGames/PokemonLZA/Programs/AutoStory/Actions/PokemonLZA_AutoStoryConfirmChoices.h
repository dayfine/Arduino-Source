/*  PokemonLZA AutoStory - Confirm Choices
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's start: the new game's "Confirm Choices" screen, after the
 *  language, look and name were chosen by hand. Its detector is in
 *  PokemonLZA_AutoStoryIntroScreens.h; 00.00's name step
 *  (type_player_name(), PokemonLZA_AutoStoryScenes.h) presses through the
 *  same screen with the same moves.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConfirmChoices_H
#define PokemonAutomation_PokemonLZA_AutoStoryConfirmChoices_H

#include <cstddef>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The highlighted row of "Confirm Choices", read from frames over up to
//  2 s with no press. Throws UserSetupError if the screen does not show.
size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context);

//  01.00's first step: the new game's language, look and name were chosen
//  by hand and "Confirm Choices" is on screen. Checks that screen first
//  (require_confirm_choices()): on any other screen throws UserSetupError
//  and presses nothing. Then moves the highlight to "Ready!" one DOWN at a
//  time, reading it after each, and presses A there: the save's only
//  change is this press, on a screen it has read. Throws
//  OperationFailedException if the highlight is lost on the way.
void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
