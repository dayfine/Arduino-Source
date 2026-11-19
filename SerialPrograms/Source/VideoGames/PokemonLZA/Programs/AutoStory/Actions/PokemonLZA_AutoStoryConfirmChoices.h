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


//  The highlighted row of "Confirm Choices", with no press. Throws UserSetupError if it does not show.
size_t require_confirm_choices(ConsoleHandle& console, ProControllerContext& context);

//  01.00b's first step: on "Confirm Choices", checks the shown name and
//  language against `choices` (the language only when its OCR is
//  installed), then moves to "Ready!" and presses A, the only press that
//  changes the save. Throws UserSetupError (not on the screen) or
//  OperationFailedException (a mismatch, or the highlight lost), having
//  pressed A only if it reached "Ready!".
void confirm_new_game_choices(ConsoleHandle& console, ProControllerContext& context, const NewGameChoices& choices);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
