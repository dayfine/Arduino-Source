/*  PokemonLZA AutoStory - Dialogs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's dialogue actions (PokemonLZA_AutoStoryActions.h has the
 *  rest): through a scene to the overworld, a talk, a choice.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDialogs_H
#define PokemonAutomation_PokemonLZA_AutoStoryDialogs_H

#include <cstddef>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a dialogue or a scene, presses `button` until the overworld is back
//  and holds for a second. Throws OperationFailedException if it is not back
//  within `timeout`.
void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout);

//  From the overworld, presses A at what the player faces. Returns once a
//  dialogue box shows; throws OperationFailedException if none does.
void talk_to_facing(ConsoleHandle& console, ProControllerContext& context);

//  With a dialogue's choices on screen, picks choice `index` (0 is the top):
//  waits for the selection arrow in dialog_choice_box()
//  (Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h), presses down until it
//  has moved `index` rows, then A. Throws OperationFailedException if the
//  arrow never shows or does not move.
void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
