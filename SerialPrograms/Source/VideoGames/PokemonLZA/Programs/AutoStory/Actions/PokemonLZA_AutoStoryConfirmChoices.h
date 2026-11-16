/*  PokemonLZA AutoStory - Confirm Choices
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The new game's "Confirm Choices" screen, after the language, look and
 *  name (PokemonLZA_AutoStoryNewGame.h). Its reader is in
 *  Detectors/PokemonLZA_AutoStoryConfirmChoicesReader.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryConfirmChoices_H
#define PokemonAutomation_PokemonLZA_AutoStoryConfirmChoices_H

#include <cstddef>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonLZA_AutoStoryNewGameChoices.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The highlighted row of "Confirm Choices", read from frames over up to
//  2 s with no press. Throws UserSetupError if the screen does not show.
size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context);

//  01.00's step after the intro: "Confirm Choices" is on screen. Checks
//  that screen first (require_confirm_choices()): on any other screen
//  throws UserSetupError and presses nothing. Then reads the name and the
//  language it shows (OCR) and, if either is not `choices`', throws
//  OperationFailedException having pressed nothing (the language is not
//  checked when its OCR is not installed). Then moves the highlight to
//  "Ready!" one DOWN at a time, reading it after each, and presses A there:
//  the save's only change is this press, on a screen it has read. Throws
//  OperationFailedException if the highlight is lost on the way.
void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
