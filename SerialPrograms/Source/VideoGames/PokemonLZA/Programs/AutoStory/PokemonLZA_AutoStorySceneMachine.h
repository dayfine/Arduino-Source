/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's scenes as ScreenMachine states (CommonTools/ScreenMachine) that
 *  match on what the backend observed (ScreenState::matches_observation),
 *  so they run on the console and on a scripted backend alike: every look
 *  picks one state, the state takes one step, and each change of state is
 *  logged with the look's events. The player has control when the party
 *  bar or, in the tutorial (which has no party bar), the controls hint
 *  (CONTROLS_HINT_READING) holds a second. A main mission's banner
 *  (MISSION_BANNER_READING) on two looks in a row ends the scene too: it
 *  shows as the scene hands over control, and the machine then waits for
 *  it to go, pressing A if it stays. The card a main mission ends with
 *  (MISSION_COMPLETE_READING) waits for A: the machine presses A at it,
 *  and it counts as busy. The advance chevron (ADVANCE_ARROW_READING) is
 *  text to press on, with or without a box. The autosave badge
 *  (AUTOSAVE_READING), then 5 s with nothing busy, ends the scene as
 *  control, before the minimum length: the game saves once a scene is over.
 *
 *  The walks between the scenes are Actions/PokemonLZA_AutoStoryWalkScreenChecks.h.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryBackend.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What a scene machine is after, and how it presses.
struct SceneMachineConfig{
    SceneMashGoal goal = SceneMashGoal::OVERWORLD;
    //  Advances the text (A or B).
    Button text_button = BUTTON_A;
    //  OVERWORLD only: fails ("scene_quiet") once nothing busy (box, choice,
    //  battle, fade) and no sign of control has shown for this long...
    std::optional<Milliseconds> quiet = std::nullopt;
    //  ...and this long has passed since the start: a cutscene can hold a
    //  still frame longer than `quiet`.
    Milliseconds min_length = Milliseconds(0);
    //  The quiet end is control, not a failure: Hotel Z's halls and rooms
    //  show no party bar or prompt (02.05, console B 2026-11-10).
    bool quiet_is_control = false;
    //  OVERWORLD with A: presses A every 2 s once nothing
    //  busy and no sign of control has shown for this long, for text the
    //  detectors miss (A does not skip a cutscene).
    std::optional<Milliseconds> blind_press_after = std::nullopt;
};

//  The states a scene machine tells apart, in the order it tries them.
enum class SceneScreen{
    GOAL_CHOICE,    //  terminal: a choice, when it is the goal
    GOAL_BATTLE,    //  terminal: the battle HUD, when it is the goal
    FADE_BLACK,     //  wait until it lifts
    FADE_WHITE,     //  wait until it lifts
    CHOICE,         //  A: the top choice
    DIALOG,         //  the text button: a box, or the advance chevron
    MISSION_COMPLETE,   //  A: a main mission's "COMPLETE" card
    MISSION_BANNER, //  terminal: a main mission's banner, on a second look
    BANNER_SHOWING, //  wait: the banner, on its first look
    CAN_MOVE,       //  terminal: the party bar, held a second
    PARTY_BAR,      //  wait: the party bar, not held a second yet
    CONTROLS_HINT,  //  terminal: the tutorial's controls hint, held a second
    HINT_SHOWING,   //  wait: the controls hint, not held a second yet
    QUIET_END,      //  terminal, failing: nothing busy for the quiet time, past the minimum length
    AUTOSAVE_QUIET, //  terminal: the autosave badge, then nothing busy for AUTOSAVE_QUIET
    BLIND_PRESS,    //  A: nothing busy and no sign of control for blind_press_after
    SCENE_MOVING,   //  no box, the screen changing: a cutscene, or the player in control
    SCENE_STILL,    //  no box, the screen still since the read before
};

//  "fade_black", the state's name in the StepTrace log.
std::string_view scene_screen_name(SceneScreen screen);

//  Whether the machine ends after this state's step.
bool scene_screen_terminal(SceneScreen screen);

//  The press a state takes: DONE for a terminal state; WAIT for a fade, the
//  party bar, and a screen with no box in an A mash (A would talk to whoever
//  stands in front); PRESS_B there in a B mash (closes the map and Pokedex
//  screens), PRESS_A when the goal is a battle (cards, the VS splash).
SceneMashAction scene_screen_action(SceneScreen screen, const SceneMachineConfig& config);

//  The time-dependent part of the machine, apart from the backend so tests
//  can feed it looks: when the screen was last busy, since when the party
//  bar shows. A look's time is its `when`.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start);

    //  The state for `look` (STILL: the screen barely changed since the
    //  look before). Does not change the timeline.
    SceneScreen classify(const ScreenObservation& look) const;

    //  Takes in the look the machine acted on.
    void record(const ScreenObservation& look);

    //  When the quiet end is due if nothing busy shows before; none
    //  without a quiet time.
    std::optional<WallClock> quiet_end_due() const;

    //  When the autosave quiet end is due if nothing busy shows before:
    //  AUTOSAVE_QUIET after the badge and after the last busy look, with no
    //  minimum length; none before the badge or outside an OVERWORLD mash.
    std::optional<WallClock> autosave_quiet_due() const;

    //  Whether a blind A press is due at `now` (blind_press_after).
    bool blind_press_due(WallClock now) const;

    const SceneMachineConfig& config() const{
        return m_config;
    }

private:
    SceneMachineConfig m_config;
    WallClock m_quiet_from;
    WallClock m_last_busy;
    //  The last look busy or with a sign of control (party bar, hint).
    WallClock m_last_cue;
    std::optional<WallClock> m_last_blind_press;
    std::optional<WallClock> m_party_bar_since;
    std::optional<WallClock> m_hint_since;
    //  The looks in a row with the mission banner.
    size_t m_banner_looks = 0;
    //  The last look with the autosave badge.
    std::optional<WallClock> m_last_autosave;

    //  classify() past the fades, boxes and the goal.
    SceneScreen classify_open(const ScreenObservation& look) const;
};


//  Presses through a scene until `config.goal` shows (OVERWORLD: the party
//  bar or the controls hint held a second, or the mission banner on two
//  looks, then up to 10 s for the banner to go and, if it stays, A up to 3
//  times 1 s apart), on run_screen_machine_on_observations().
//  `label` names its log lines ("[PLZA opening] dialog: text box"). Fails as
//  "scene_quiet" on the quiet end, "scene_timeout" when `timeout` passes (by the looks' time),
//  "scene_unknown" on a look no state takes and "scene_stuck" on a state
//  that comes up too often or a screen that stops changing.
Pokemon::Outcome run_scene_machine(
    PlzaBackend& backend, std::string_view label, const SceneMachineConfig& config, Milliseconds timeout
);

//  Registers the scene machine's tests: its states on sequences of events,
//  no frames needed.
void add_tests_AutoStorySceneMachine(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
