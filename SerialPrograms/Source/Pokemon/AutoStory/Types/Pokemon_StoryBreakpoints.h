/*  Story Breakpoints
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A debugger's breakpoints for a story run: the program pauses before each
 *  step, or before each leg of a walk too, so the user can look at the
 *  console and then continue. The engine and the walk action call
 *  StoryPausePoints at those points; what a pause does is the game's
 *  StoryGame::pause_point hook. A normal build installs no wait
 *  (set_story_pause_wait()), so a pause point costs a log line and never
 *  blocks; a lab build installs one that waits for the user.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryBreakpoints_H
#define PokemonAutomation_Pokemon_StoryBreakpoints_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include "Common/Cpp/CancellableScope.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Where a run pauses.
enum class StoryBreakpoints{
    //  Nowhere.
    OFF,
    //  Before each step.
    STEPS,
    //  Before each step and each leg of a walk.
    LEGS,
};

// Returns the wire and log name of a mode: "off", "steps", "legs".
std::string story_breakpoints_name(StoryBreakpoints mode);

//  "12.03/5 pallet_shore": the checkpoint, the step's place in it (from 1)
//  and its name.
std::string story_step_pause_label(std::string_view checkpoint_id, size_t step_index, std::string_view step_name);

//  "leg 2/5 east 4 -> (10, 10)": leg `index` (from 0) of `count`, and the
//  tile it ends on when the walk started on one known tile.
std::string story_leg_pause_label(
    size_t index, size_t count, Direction direction, int tiles, const std::optional<TilePos>& destination
);


//  What a pause point does when it is reached, given its full label. It
//  returns when the run goes on, or when `scope` is cancelled.
using StoryPauseHook = std::function<void(std::string_view label)>;

//  The pause points of one run. The engine owns it and hands it to actions.
class StoryPausePoints{
public:
    StoryPausePoints() = default;
    StoryPausePoints(StoryBreakpoints mode, StoryPauseHook hook);

    //  Where the engine logs "[Break]" lines.
    void set_log(std::function<void(std::string_view line)> log);

    //  A step is about to run. Remembers `label` for the legs of the step
    //  and pauses when the mode asks for steps.
    void before_step(const std::string& label);
    //  A leg is about to be walked: pauses, as "<the step's label> <leg_label>",
    //  only in LEGS mode.
    void before_leg(const std::string& leg_label);

private:
    void pause(const std::string& label);

    StoryBreakpoints m_mode = StoryBreakpoints::OFF;
    StoryPauseHook m_hook;
    std::function<void(std::string_view line)> m_log;
    std::string m_step_label;
};


//  The wait a lab build installs, called with the label and the run's scope.
//  It returns when the user continues or `scope` is cancelled. Set once,
//  before any program runs. Not installed: pause points do not block.
using StoryPauseWait = std::function<void(std::string_view label, CancellableScope& scope)>;
void set_story_pause_wait(StoryPauseWait wait);

//  The hook for StoryGame::pause_point: the installed wait on `scope`, or
//  nothing when none is installed. `scope` must outlive the hook.
StoryPauseHook make_story_pause_hook(CancellableScope& scope);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
