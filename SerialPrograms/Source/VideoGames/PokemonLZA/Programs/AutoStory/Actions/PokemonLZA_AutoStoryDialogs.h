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
#include <functional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogDetector.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a dialogue or a scene, runs `presses` (repeating it) until the
//  overworld is back: OverworldPartySelectionWatcher held OVERWORLD_HOLD.
//  Throws OperationFailedException with `failure` if it is not back within
//  `timeout`.
void press_until_overworld(
    ConsoleHandle& console, ProControllerContext& context,
    const std::function<void(ProControllerContext&)>& presses,
    Milliseconds timeout, std::string_view failure
);

//  press_until_overworld() with `button` mashed.
void mash_to_overworld(ConsoleHandle& console, ProControllerContext& context, Button button, Milliseconds timeout);

//  What press_a_until_quiet() pressed A through.
struct ScreensPassed{
    bool card = false;
    bool text = false;
    bool timed_out = false;     //  still busy at the timeout
};

//  A through tutorial cards and the dialogue boxes of `kinds`
//  (StoryDialogDetector) until none has shown for `quiet`, or `timeout`.
ScreensPassed press_a_until_quiet(
    ConsoleHandle& console, ProControllerContext& context,
    StoryDialogKinds kinds, Milliseconds quiet, Milliseconds timeout
);

//  From the overworld, presses A at what the player faces. Returns once a
//  dialogue box shows; throws OperationFailedException if none does.
void talk_to_facing(ConsoleHandle& console, ProControllerContext& context);

//  The D-pad presses from choice row `current` to row `target`: down when
//  positive, up when negative.
int dialog_choice_moves(size_t current, size_t target);

//  With a dialogue's choices on screen, picks choice `index` (0 is the top):
//  waits for the selection arrow in dialog_choice_box()
//  (Detectors/PokemonLZA_AutoStoryDialogChoiceBox.h), reads its row with
//  get_current_selector_index() (Programs/PokemonLZA_FastTravelNavigation.h)
//  on dialog_choice_rows_box(), presses the difference, reads the row
//  again, then A. Throws OperationFailedException if the arrow never shows,
//  is outside the rows, or is not on row `index` after the presses.
void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
