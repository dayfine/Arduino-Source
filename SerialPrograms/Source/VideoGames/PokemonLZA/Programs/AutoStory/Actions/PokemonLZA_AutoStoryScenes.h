/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions that press through scenes rather than walk: the scene
 *  mash over PlzaScreenObserver's events, and the ladder, which is timed
 *  presses as recorded on console B.
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
//  120 s (a scene can hold a ~25 s black screen: 02.07's first evening),
//  and the scene must start (SceneStart) within SCENE_START_WINDOW: a run
//  not at the scene's trigger fails there, before an A mash talks to
//  whoever stands in front.
void mash_story_scene(ConsoleHandle& console, ProControllerContext& context, Button text_button);


//  Taunie's talk after her battle (03.02): A through it and the "Map &
//  Missions" card (A turns its pages, B turns back) until the map shows,
//  B to close the map (A would place a pin), then B through the rest of the
//  talk to the overworld (console B, 2026-11-10). Throws
//  OperationFailedException if the map does not show within 60 s.
void talk_through_map_screen(ConsoleHandle& console, ProControllerContext& context);

//  Facing the side street's wall (03.03): sidestep right to the ladder
//  (0.6 s), face it (150 ms), A at "A Climb Up", hold up 5 s to the top.
void climb_ladder(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
