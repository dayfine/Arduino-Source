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
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryDialogDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How long the party bar (OverworldPartySelectionDetector) must hold to be
//  the overworld: it also shows between the lines of a scene.
constexpr Milliseconds OVERWORLD_HOLD(1000);

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

//  Where a dialogue's choices are drawn, and choose_dialog_option() looks
//  for the selection arrow: the restaurant receptionist's (RestaurantFarmer).
//  Other dialogues' choices are assumed to sit there.
const ImageFloatBox& dialog_choice_box();

//  With a dialogue's choices on screen, picks choice `index` (0 is the top):
//  waits for the selection arrow, presses down until it has moved `index`
//  rows, then A. Throws OperationFailedException if the arrow never shows or
//  does not move.
void choose_dialog_option(ConsoleHandle& console, ProControllerContext& context, size_t index);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
