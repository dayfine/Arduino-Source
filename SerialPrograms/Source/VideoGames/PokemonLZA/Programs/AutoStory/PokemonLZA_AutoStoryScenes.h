/*  PokemonLZA AutoStory - Scenes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story actions that press through scenes rather than walk: the scene
 *  mash over PlzaScreenObserver's events, and the new game's opening and
 *  the ladder, which are timed presses as recorded on console B
 *  (2026-11-08). None has run on a console as a story step yet.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryScenes_H
#define PokemonAutomation_PokemonLZA_AutoStoryScenes_H

#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

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
//  Throws OperationFailedException if the goal does not show within `timeout`.
void mash_scene(
    ConsoleHandle& console, ProControllerContext& context,
    SceneMashGoal goal, Button text_button, Milliseconds timeout,
    std::optional<Milliseconds> quiet = std::nullopt,
    Milliseconds min_length = Milliseconds(0)
);


//  The new game's opening, as recorded 2026-11-08 on console B. None of it
//  reads the screen: each waits a fixed time for its screen.

//  At the title screen with no save: A into the language list, A on its
//  first entry (English, the default).
void new_game_language(ConsoleHandle& console, ProControllerContext& context);

//  On the look picker, whose highlight starts at random: UP, then LEFT x3
//  to the first look (the edges clamp), A.
void pick_first_look(ConsoleHandle& console, ProControllerContext& context);

//  On the name screen: types `name` on the Switch keyboard (upper case
//  letters and digits only, keyboard_enter_code()), + to finish, then
//  DOWN x2 and A on "Ready!".
void type_player_name(ConsoleHandle& console, ProControllerContext& context, const std::string& name);

//  Facing the side street's wall (03.03): sidestep right to the ladder
//  (0.6 s), face it (150 ms), A at "A Climb Up", hold up 5 s to the top.
void climb_ladder(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
