/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions that press through scenes rather than walk: the scene
 *  mash over the story backend's looks (SceneMash, on the shared TalkOver),
 *  and the ladder, which is timed presses as recorded on console B.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include <optional>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  From a scene, presses through it as SceneMash says on `backend`'s
//  looks, until `goal` shows (OVERWORLD: the party bar held a second, by
//  the shared TalkOver). `text_button` advances the text (A or B).
//  A tutorial card (TUTORIAL_CARD_READING) gets A whatever `text_button`
//  is: only A turns its pages. With a `start_window`, the scene must start
//  (SceneStart) within it. Fails as "scene_never_started" when it does not,
//  and as "scene_timeout" when the goal does not show within `timeout`
//  (by the looks' time).
Pokemon::Outcome mash_scene(
    PlzaBackend& backend, SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> start_window = std::nullopt
);

//  A "scene" step: mash_scene() to the overworld with `text_button`, up to
//  120 s (a scene can hold a ~25 s black screen: 02.07's first evening),
//  and the scene must start within SCENE_START_WINDOW: a run not at the
//  scene's trigger fails there, before an A mash talks to whoever stands
//  in front.
Pokemon::Outcome mash_story_scene(PlzaBackend& backend, Button text_button);


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
