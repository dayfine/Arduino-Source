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
 *  (ADVANCE_ARROW_READING) gets A. The player has control when the party
 *  bar (PLAYER_CAN_MOVE) or, in the tutorial, which has no party bar, the
 *  controls hint (CONTROLS_HINT_READING) holds a second, or when
 *  a main mission's banner (MISSION_BANNER_READING) shows on two looks in a
 *  row: it shows as the scene hands over control, and the machine then
 *  waits for it to go, pressing A if it stays (01.03's "MAIN MISSION 02").
 *  01.01's arrival scenes end on one or the other. The card a main mission
 *  ends with (MISSION_COMPLETE_READING) waits for A: the machine presses A
 *  at it, and it counts as busy. A machine that stops at a choice
 *  (SceneMachineConfig::stop_at_choice) ends when the choice box shows
 *  instead: 01.02's "Battle with Pokemon!"; one that stops at a battle
 *  (stop_at_battle) ends on the battle HUD (BATTLE_STARTED): 01.03's
 *  battle with Andi; neither takes the party bar for control, as it shows
 *  between the lines of a talk.
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
    //  Presses `blind_button` every 2 s once nothing busy and no sign of
    //  control has shown for this long, for text the detectors miss (01.01's
    //  ad). A does not skip a cutscene.
    std::optional<Milliseconds> blind_press_after = std::nullopt;
    //  The button of those blind presses: B on a screen that A does not
    //  close (03.04's Pokedex entry, where A only plays its motions).
    Button blind_button = BUTTON_A;
    //  Ends at the choice box (CHOICE_BOX) instead of taking its top row.
    bool stop_at_choice = false;
    //  Ends at the battle HUD (BATTLE_STARTED), before anything else.
    bool stop_at_battle = false;
    //  The button that advances text: B where an A could land on a choice
    //  that shows the moment the text ends.
    Button text_button = BUTTON_A;
    //  The quiet end is control, not a failure: Hotel Z's lobby, halls and
    //  rooms show no party bar or prompt (02.04-02.06, console B
    //  2026-11-10).
    bool quiet_is_control = false;
    //  Fails ("scene_never_started") when no text, choice or battle has
    //  shown this long after the start, and no sign of control either: the
    //  player is not at the scene's trigger (02.02 run after the Brigitte
    //  battle walked past the street scene's, console B 2026-11-13).
    std::optional<Milliseconds> start_window = std::nullopt;
    //  Ends once this many black fades have been waited out and the screen
    //  has then shown no text, fade or banner for 5 s (or on the controls
    //  hint, if it comes first), and takes the mission banner for part of
    //  the scene, not its end; whether it showed is logged. A banner up 2 s
    //  gets A, up to 3 times 1 s apart (the Italian one stays until A,
    //  console B 2026-11-15), and one that stays fails. 01.01's arrival
    //  ends on its third black fade after the ad, which follows the banner
    //  (console B, 2026-11-15, English and Italian; the banner detector
    //  missed it on the Italian game). Text after the last fade starts the
    //  5 s again.
    std::optional<size_t> end_after_black_fades = std::nullopt;
};

//  Presses through scenes until the controls hint has held a second, or
//  the mission banner has shown on two looks (then waits up to 10 s for it
//  to go and, if it stays, presses A up to 3 times 1 s apart), or with
//  `config.stop_at_choice` the choice box shows, or with
//  `config.stop_at_battle` the battle HUD, or with
//  `config.end_after_black_fades` its last fade has lifted and settled, on
//  run_screen_machine(). `label` names its log lines ("[PLZA
//  mash_to_move_hints] screen 2: dialog at 1.5s: text box"), which go to
//  the file log (log_detail()) except the black fades, the banner, the
//  hint and the end, which go on the overlay too (log()). Fails as "scene_quiet" on the
//  quiet end (unless `config.quiet_is_control`), "scene_never_started"
//  past `config.start_window`, "scene_timeout" when `timeout` passes (by
//  the looks' time), and "scene_stuck" on a state that comes up too often
//  or a screen that stops changing.
Pokemon::Outcome run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
