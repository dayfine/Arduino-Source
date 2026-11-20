/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Presses through a run of scenes to free control: A by default, a few
 *  screens that take another press, and an end once a sign of control holds.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Actions/PokemonLZA_AutoStoryHeading.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  How long a Plus press is held to skip a cutscene, as upstream's
//  AutoStory (#1933) holds it.
constexpr Milliseconds CUTSCENE_SKIP_HOLD = Milliseconds(2000);

//  What ends a scene machine besides control, and its exceptions to A.
struct SceneMachineConfig{
    //  Ends at a choice box, for the step after to pick its row. Text gets B
    //  then, and other screens nothing: an A in flight as the choice opens
    //  would take its top row...
    bool stop_at_choice = false;
    //  ...unless B closes the talk and the step takes the top row anyway (the
    //  elevators): one A per text page.
    bool text_a_to_choice = false;
    //  Ends at the battle HUD or a lock-on. The HUD shows only while locked
    //  on, so after a black fade a still screen (3 s) or the party bar (1 s)
    //  gets ZL held while `lock_on` watches for the move symbols, then up to
    //  4 quarter turns + L. A cutscene (moving, no party bar) gets no ZL.
    bool stop_at_battle = false;
    //  Ends at the battle HUD (battle_filter()) as battle_on_screen_failure(),
    //  for a step before the checkpoint's battle step: a trainer's battle can
    //  start before the plan's walk to them (03.02b), and A there is a move.
    bool end_at_battle = false;
    //  None: no lock-on (a backend that models none).
    std::optional<LockOnWatch> lock_on = std::nullopt;
    //  For text and screens no detector reads: B where A does something else
    //  (03.04's Pokedex plays the entry's motions; Mable's menus in 03.10/03.11).
    Button button = BUTTON_A;
    //  The step plays a scene: the HUD ends it only once a scene is under way.
    //  Fails ("scene_never_started") when the HUD shows this long with no
    //  scene: the player is not at the trigger.
    std::optional<Milliseconds> start_window = std::nullopt;
};

//  Text, a choice, a battle, a tutorial card, or with no HUD up a fade or a
//  moving screen (a cutscene can play a minute with no text).
bool scene_under_way(const ScreenObservation& look);

//  A text box, or the advance chevron without one.
bool text_shows(const ScreenObservation& look);

//  A black or a white fade.
bool fade_shows(const ScreenObservation& look);

//  The party bar with no text box over it (it shows between a talk's lines too).
bool party_bar_free(const ScreenObservation& look);

//  "text box, advance arrow": `look`'s events and readings; "nothing" for none.
std::string describe_scene_look(const ScreenObservation& look);

//  Presses through scenes until the party bar, the controls hint or an A
//  prompt (none under a text box) holds 1 s. Each look: a choice takes its
//  top row, but B when it shows again (same row count) after A took it:
//  A would loop the talk (03.05's nurse, healing again); A if it is still
//  up after that B (03.11's Mable asks two questions). A map gets B, a
//  tutorial card A; text and anything else A, but B on a look with a sign
//  of control and on every look once one has shown in the scene: A there
//  starts a talk (03.02b's Vinci loop). Fades only wait.
//  Fails as "scene_never_started", "scene_ended_in_control" or
//  "scene_timeout" (by the looks' time); `step` is "<checkpoint>/<step>".
std::optional<Pokemon::StoryFailure> run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout, std::string_view step
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
