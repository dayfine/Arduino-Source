/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A run of scenes pressed through to free control, as ScreenMachine
 *  states (CommonTools/ScreenMachine) that match on what the backend
 *  observed, so they run on the console and on a scripted backend alike:
 *  every look picks one state, the state takes one step, and each change of
 *  state is logged with the look's events. A text box (with or without a
 *  choice, whose top row A takes) or the advance chevron
 *  (ADVANCE_ARROW_READING) gets A. The player has control when the
 *  tutorial's controls hint (CONTROLS_HINT_READING) holds a second, or when
 *  a main mission's banner (MISSION_BANNER_READING) shows on two looks in a
 *  row: it shows as the scene hands over control, and the machine then
 *  waits for it to go. 01.01's arrival scenes end on one or the other.
 *  A machine that stops at a choice (SceneMachineConfig::stop_at_choice)
 *  ends when the choice box shows instead: 01.02's "Battle with Pokemon!".
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
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
    //  Presses A every 2 s once nothing busy and no sign of control has
    //  shown for this long, for text the detectors miss (01.01's ad). A does
    //  not skip a cutscene.
    std::optional<Milliseconds> blind_press_after = std::nullopt;
    //  Ends at the choice box (CHOICE_BOX) instead of taking its top row.
    bool stop_at_choice = false;
    //  The button that advances text: B where an A could land on a choice
    //  that shows the moment the text ends.
    Button text_button = BUTTON_A;
};

//  The states a scene machine tells apart, in the order it tries them.
enum class SceneScreen{
    FADE_BLACK,     //  wait until it lifts
    FADE_WHITE,     //  wait until it lifts
    CHOICE,         //  terminal: the choice box, with stop_at_choice
    DIALOG,         //  the text button: a box (A: its choice's top row too), or the advance chevron
    MISSION_BANNER, //  terminal: a main mission's banner, on a second look
    BANNER_SHOWING, //  wait: the banner, on its first look
    CONTROLS_HINT,  //  terminal: the tutorial's controls hint, held a second
    HINT_SHOWING,   //  wait: the controls hint, not held a second yet
    QUIET_END,      //  terminal, failing: nothing busy for the quiet time, past the minimum length
    BLIND_PRESS,    //  A: nothing busy and no sign of control for blind_press_after
    SCENE_MOVING,   //  wait: no text, the screen changing
    SCENE_STILL,    //  wait: no text, the screen still since the look before
};

//  "fade_black", the state's name in the StepTrace log.
std::string_view scene_screen_name(SceneScreen screen);

//  Whether the machine ends after this state's step.
bool scene_screen_terminal(SceneScreen screen);

//  Whether this state's step presses (DIALOG the config's text button,
//  BLIND_PRESS A); the others wait.
bool scene_screen_presses(SceneScreen screen);

//  The time-dependent part of the machine, apart from the backend so tests
//  can feed it looks: when the screen was last busy, since when the hint
//  shows. A look's time is its `when`.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start);

    //  The state for `look`. Does not change the timeline.
    SceneScreen classify(const ScreenObservation& look) const;

    //  Takes in the look the machine acted on.
    void record(const ScreenObservation& look);

    //  When the quiet end is due if nothing busy shows before; none
    //  without a quiet time.
    std::optional<WallClock> quiet_end_due() const;

    //  Whether a blind A press is due at `now` (blind_press_after).
    bool blind_press_due(WallClock now) const;

private:
    //  classify() past the fades and the text.
    SceneScreen classify_open(const ScreenObservation& look) const;

    SceneMachineConfig m_config;
    WallClock m_quiet_from;
    WallClock m_last_busy;
    //  The last look busy or with a sign of control (hint, banner).
    WallClock m_last_cue;
    std::optional<WallClock> m_last_blind_press;
    std::optional<WallClock> m_hint_since;
    //  The looks in a row with the mission banner.
    size_t m_banner_looks = 0;
};


//  Presses through scenes until the controls hint has held a second, or
//  the mission banner has shown on two looks (then waits up to 10 s for it
//  to go), or with `config.stop_at_choice` the choice box shows, on
//  run_screen_machine(). `label` names its log lines ("[PLZA
//  mash_to_move_hints] dialog: text box"). Fails as "scene_quiet" on the
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
