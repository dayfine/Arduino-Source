/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of scenes pressed through to free control, as ScreenMachine
 *  states (CommonTools/ScreenMachine) that match on what the backend
 *  observed, so they run on the console and on a scripted backend alike:
 *  every look picks one state, the state takes one step, and each change of
 *  state or of the look's events is logged (repeats are summed up). A text box (with or without a
 *  choice, whose top row A takes) or the advance chevron
 *  (ADVANCE_ARROW_READING) gets A. The player has control when the
 *  tutorial's controls hint (CONTROLS_HINT_READING) holds a second, or when
 *  a main mission's banner (MISSION_BANNER_READING) shows on two looks in a
 *  row: it shows as the scene hands over control, and the machine then
 *  waits for it to go, pressing A if it stays (01.03's "MAIN MISSION 02").
 *  01.01's arrival scenes end on one or the other. The card a main mission
 *  ends with (MISSION_COMPLETE_READING) waits for A: the machine presses A
 *  at it, and it counts as busy. A machine that stops at a choice
 *  (SceneMachineConfig::stop_at_choice) ends when the choice box shows
 *  instead: 01.02's "Battle with Pokemon!"; one that stops at a battle
 *  (stop_at_battle) ends on the battle HUD (BATTLE_STARTED): 01.03's
 *  battle with Andi.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  When a scene machine gives up on a screen with nothing going on, and
//  when it presses A blind.
struct SceneMachineConfig{
    //  Fails ("scene_quiet") once nothing busy (text, a fade) and no sign of
    //  control has shown for this long...
    std::optional<Milliseconds> quiet = std::nullopt;
    //  ...and this long has passed since the start: a cutscene can hold a
    //  still frame longer than `quiet`.
    Milliseconds min_length = Milliseconds(0);
    //  Presses A every 2 s once nothing busy and no sign of control has
    //  shown for this long, for text the detectors miss (01.01's ad). A does
    //  not skip a cutscene.
    std::optional<Milliseconds> blind_press_after = std::nullopt;
    //  Ends at the choice box (CHOICE_BOX) instead of taking its top row.
    bool stop_at_choice = false;
    //  Ends at the battle HUD (BATTLE_STARTED), before anything else.
    bool stop_at_battle = false;
    //  The button that advances text: B where an A could land on a choice
    //  that shows the moment the text ends.
    Button text_button = BUTTON_A;
};

//  Presses through scenes until the controls hint has held a second, or
//  the mission banner has shown on two looks (then waits up to 10 s for it
//  to go and, if it stays, presses A up to 3 times 1 s apart), or with
//  `config.stop_at_choice` the choice box shows, or with
//  `config.stop_at_battle` the battle HUD, on
//  run_screen_machine(). `label` names its log lines ("[PLZA
//  mash_to_move_hints] screen 2: dialog at 1.5s: text box"). Fails as "scene_quiet" on the
//  quiet end, "scene_timeout" when `timeout` passes (by the looks' time),
//  and "scene_stuck" on a state that comes up too often or a screen that
//  stops changing.
Pokemon::Outcome run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
