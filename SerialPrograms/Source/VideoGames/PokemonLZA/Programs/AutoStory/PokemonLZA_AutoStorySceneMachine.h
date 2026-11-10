/*  PokemonLZA AutoStory - Scene machine
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  01.00's scenes as a ScreenMachine (CommonTools/ScreenMachine): every read
 *  of the screen picks one state from PlzaScreenObserver's events, the state
 *  takes one step, and StepTrace logs each change of state with its frame.
 *  That log says when the player gets control, which the tutorial shows with
 *  no party bar (only the Move / Control Camera hints, which have no detector
 *  yet: no frame of them has been kept).
 *
 *  The walks between the scenes start only on a screen with no box, choice,
 *  fade or battle, and a walk that ends on a fade stops on it.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H
#define PokemonAutomation_PokemonLZA_AutoStorySceneMachine_H

#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_ScreenEvents.h"
#include "PokemonLZA_AutoStoryMoves.h"
#include "PokemonLZA_AutoStoryScreenEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  What a scene machine is after, and how it presses.
struct SceneMachineConfig{
    SceneMashGoal goal = SceneMashGoal::OVERWORLD;
    //  Advances the text (A or B).
    Button text_button = BUTTON_A;
    //  OVERWORLD only: also done once nothing busy (box, choice, battle,
    //  fade) has shown for this long...
    std::optional<Milliseconds> quiet = std::nullopt;
    //  ...and this long has passed since the start: a cutscene can hold a
    //  still frame longer than `quiet`.
    Milliseconds min_length = Milliseconds(0);
};

//  The states a scene machine tells apart, in the order it tries them.
enum class SceneScreen{
    GOAL_CHOICE,    //  terminal: a choice, when it is the goal
    GOAL_BATTLE,    //  terminal: the battle HUD, when it is the goal
    FADE_BLACK,     //  wait until it lifts
    FADE_WHITE,     //  wait until it lifts
    CHOICE,         //  A: the top choice
    DIALOG,         //  the text button
    CAN_MOVE,       //  terminal: the party bar, held a second
    PARTY_BAR,      //  wait: the party bar, not held a second yet
    QUIET_END,      //  terminal: nothing busy for the quiet time, past the minimum length
    SCENE_MOVING,   //  no box, the screen changing: a cutscene, or the player in control
    SCENE_STILL,    //  no box, the screen still since the read before
};

//  "fade_black", the state's name in the StepTrace log.
const char* scene_screen_name(SceneScreen screen);

//  Whether the machine ends after this state's step.
bool scene_screen_terminal(SceneScreen screen);

//  The press a state takes: DONE for a terminal state; WAIT for a fade, the
//  party bar, and a screen with no box in an A mash (A would talk to whoever
//  stands in front); PRESS_B there in a B mash (closes the map and Pokedex
//  screens), PRESS_A when the goal is a battle (cards, the VS splash).
SceneMashAction scene_screen_action(SceneScreen screen, const SceneMachineConfig& config);

//  The time-dependent part of the machine, apart from the console so tests
//  can feed it events: when the screen was last busy, since when the party
//  bar shows.
class SceneTimeline{
public:
    SceneTimeline(SceneMachineConfig config, WallClock start);

    //  The state for a read showing `events` at `now`; `still`: the screen
    //  barely changed since the read before. Does not change the timeline.
    SceneScreen classify(const Pokemon::ScreenEvents& events, bool still, WallClock now) const;

    //  Takes in the read the machine acted on.
    void record(const Pokemon::ScreenEvents& events, WallClock now);

    //  When the quiet end is due if nothing busy shows before; none
    //  without a quiet time.
    std::optional<WallClock> quiet_end_due() const;

    const SceneMachineConfig& config() const{
        return m_config;
    }

private:
    SceneMachineConfig m_config;
    WallClock m_quiet_from;
    WallClock m_last_busy;
    std::optional<WallClock> m_party_bar_since;
};


//  Presses through a scene as a ScreenMachine until `config.goal` shows
//  (OVERWORLD: the party bar held a second, or the quiet end). `label`
//  names the StepTrace (its log lines and its frame folder). Throws
//  OperationFailedException on a timeout or a state that comes up too often.
void run_scene_machine(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& label, const SceneMachineConfig& config, Milliseconds timeout
);

//  Throws OperationFailedException unless the screen shows no box, choice,
//  fade or battle on two reads: a walk started anywhere else walks blind.
//  The tutorial has no party bar, so it is not required.
void check_can_walk(ConsoleHandle& console, ProControllerContext& context, const std::string& walk);

//  Walks `move`, holding its last leg until `end` shows (a fade), the leg's
//  time as the upper bound. Throws OperationFailedException if `end` does
//  not show within it.
void walk_until(
    ConsoleHandle& console, ProControllerContext& context,
    const std::string& walk, const StoryMove& move, Pokemon::ScreenEvent end
);

//  Registers the scene machine's tests: its states on sequences of events,
//  no frames needed.
void add_tests_AutoStorySceneMachine(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
