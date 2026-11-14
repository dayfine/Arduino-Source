/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions that press through scenes rather than walk: the scene
 *  mash over PlzaScreenObserver's events.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a scene, presses through it as scene_mash_action() says, reading
//  PlzaScreenObserver's events, until `goal` shows (OVERWORLD: the party
//  bar held a second). `text_button` advances the text (A or B). With a
//  `quiet` time, an OVERWORLD mash also ends once no box, choice, battle or
//  fade has shown for that long: the new game's tutorial shows no party bar
//  (only the Move / Control Camera hints, which have no detector yet).
//  A cutscene can hold a still frame longer than `quiet`, so the quiet end
//  waits for `min_length`, the scene's rough recorded length, to pass.
//  A tutorial card gets A whatever `text_button` is: only A turns its pages
//  (tutorial_card_shows(), Detectors/PokemonLZA_AutoStoryTutorialCardDetector.h).
//  Throws OperationFailedException if the goal does not show within `timeout`.
void mash_scene(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> quiet = std::nullopt,
    Milliseconds min_length = Milliseconds(0)
);

//  A "scene" step: mash_scene() to the overworld with `text_button`, up to
//  120 s (a scene can hold a ~25 s black screen: 02.06's first evening),
//  and the scene must start (SceneStart) within SCENE_START_WINDOW: a run
//  not at the scene's trigger fails there, before an A mash talks to
//  whoever stands in front.
void mash_story_scene(ConsoleHandle& console, ProControllerContext& context, Button text_button);




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
