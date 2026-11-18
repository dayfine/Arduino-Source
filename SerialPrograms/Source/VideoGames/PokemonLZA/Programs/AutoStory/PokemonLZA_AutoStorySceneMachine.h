/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of scenes pressed through to free control, as ScreenMachine
 *  states (CommonTools/ScreenMachine) that match on what the backend
 *  observed, so they run on the console and on a scripted backend alike:
 *  every look picks one state, the state takes one step, and each change of
 *  state or of the look's events is logged (repeats are summed up). The
 *  end rules are run_scene_machine()'s, the knobs SceneMachineConfig's.
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
    //  Presses `blind_button` every `blind_press_every` once nothing busy
    //  and no sign of control has shown for this long, for a screen no
    //  detector reads (03.04's Pokedex screens). Nothing else is pressed
    //  blind: a blind A at the end of a talk restarts it.
    std::optional<Milliseconds> blind_press_after = std::nullopt;
    //  How often those blind presses come; zero: on every look, which
    //  closes the menus a talk opens (03.11's Mable's screens).
    Milliseconds blind_press_every = Milliseconds(2000);
    //  Once a scene is under way, one blind press when nothing busy and no
    //  sign of control has shown for this long, and again only after the
    //  next cue: a box no detector reads inside a scene (03.07's "received
    //  10 Poke Balls").
    std::optional<Milliseconds> gap_press = std::nullopt;
    //  The button of the blind presses: B on a screen A does not close
    //  (03.04's Pokedex entry, where A only plays its motions).
    Button blind_button = BUTTON_A;
    //  Ends at the choice box (CHOICE_BOX) instead of taking its top row.
    bool stop_at_choice = false;
    //  Ends at the battle HUD (BATTLE_STARTED), before anything else, or
    //  at a lock-on. The HUD shows only while the player is locked on:
    //  after a black fade with no text since, a still screen with nothing
    //  busy for 3 s or the party bar held a second gets ZL held while
    //  `lock_on` watches for the move symbols, then up to 4 times a
    //  quarter turn, L and that again. The symbols end the machine
    //  (locked on); a cutscene (the screen moving, no party bar) gets no
    //  ZL, and text starts the tries again.
    bool stop_at_battle = false;
    //  With stop_at_battle, the lock-on's watch; none: no lock-on (a
    //  backend that models none).
    std::optional<LockOnWatch> lock_on = std::nullopt;
    //  The button that advances text in a machine that stops at a choice
    //  or a battle, and the advance chevron with no box: B where an A could
    //  land on a choice that shows the moment the text ends. A text box in
    //  any other machine gets B (it cannot talk again once the box
    //  closes), a choice A.
    Button text_button = BUTTON_A;
    //  A, not B, on every text box, also in a machine that ends in
    //  control: for a scene whose choice a B would answer with its cancel
    //  row as the box opens (03.01's lobby). An A on a closing box can
    //  start the talk again, so only scenes that end with no A prompt set it.
    bool text_with_a = false;
    //  The quiet end is control, not a failure: Hotel Z shows no party bar
    //  or prompt (02.04-02.06).
    bool quiet_is_control = false;
    //  The step plays a scene: the HUD (the party bar, the controls hint,
    //  an A prompt) ends it only once a scene is under way
    //  (scene_under_way()). Fails ("scene_never_started") when the HUD
    //  shows this long after the start with no scene: the player is not at
    //  the scene's trigger.
    std::optional<Milliseconds> start_window = std::nullopt;
    //  Ends once this many black fades have been waited out and the screen
    //  has then shown no text, fade or banner for 5 s (or on the controls
    //  hint, if it comes first), and takes the mission banner for part of
    //  the scene, not its end. A banner up 2 s gets A, up to 3 times 1 s
    //  apart, and one that stays fails. 01.01's arrival ends on its third
    //  black fade after the ad, which follows the banner (the banner
    //  detector misses it on the Italian game). Text after the last fade
    //  starts the 5 s again.
    std::optional<size_t> end_after_black_fades = std::nullopt;
};

//  A scene is under way on `look`: text (a box, or a subtitle's advance
//  chevron), a choice, a battle or a tutorial card, or, with no HUD up (the
//  party bar, the controls hint, an A prompt), a fade or a moving screen:
//  a cutscene can play a minute with no text (02.02's street scene).
//  "talk" takes it for its talk having started: Taunie's (03.02b) opens on
//  a dimmed card with no text box.
bool scene_under_way(const ScreenObservation& look);

//  "text box, advance arrow": `look`'s events and readings, as the scene
//  machine logs them; "nothing" for none.
std::string describe_scene_look(const ScreenObservation& look);

//  Logs where the step should end, then presses through scenes until
//  control (run_screen_machine()). A choice takes its top row: Up while a
//  lower row is highlighted, then A. A text box gets B in a machine that
//  ends in control, else `config.text_button`, as does the advance
//  chevron; a tutorial card and the mission complete card get A. Control:
//  the party bar or the controls hint (the tutorial has no party bar)
//  held a second, or an A prompt held a second past `config.min_length`
//  (01.02's battle area shows only the prompt to Ryon), or the mission
//  banner on two looks (then up to 10 s for it to go, A up to 3 times 1 s
//  apart if it stays). With `config.stop_at_choice` it ends on the choice
//  box, with `config.stop_at_battle` on the battle HUD, and neither takes
//  the party bar or a prompt for control (they show between a talk's
//  lines; after the battle's fade a battle start locks on to bring the
//  HUD up, and ends once locked on: SceneMachineConfig::stop_at_battle); with
//  `config.end_after_black_fades` once its last fade has lifted and
//  settled. `label` names its log lines ("[PLZA
//  mash_to_move_hints] screen 2: dialog at 1.5s: text box"), which go to
//  log_detail() except the black fades, the banner, the hint and the end
//  (log()). Fails as "scene_quiet" on the quiet end (unless
//  `config.quiet_is_control`), "scene_never_started" on the HUD past
//  `config.start_window` with no scene, "scene_timeout" when `timeout`
//  passes (by the looks' time), and "scene_stuck" on a state that comes up
//  too often or a screen that stops changing.
Pokemon::Outcome run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
