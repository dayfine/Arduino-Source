/*  BDSP AutoStory - Story executor
 *
// code-health: allow header_symbols -- BDSP story executor and checkpoint execution primitives
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's way of doing the shared story steps
 *  (Pokemon/AutoStory/Pokemon_StorySteps.h) on the console. A route is
 *  planned on the joined maps that hold its start and its target
 *  (story_plan_world()), from the tiles the run's pose holds, and walked by
 *  the walker, which fights the trainers who see us and runs from wild
 *  battles. A step whose variant names one of the story's events
 *  (story_events(), PokemonBDSP_AutoStory_Events.h) runs that event,
 *  whatever its kind: the scripted scenes and the console-measured walks
 *  of segment 01. It is the console's binding of the story's action table
 *  (story_action_table()); the simulation binds the same table.
 *
 *  Every step runs inside a guard (ConsoleStepGuard) that turns a failed
 *  console step into a failed Outcome, with a screenshot and the
 *  "checkpoint_reattempt_loop: attempt N failed" line the lab's run watcher
 *  reads; the segment then reloads the row's save and runs it again.
 *  Before each step it clears what came up since the step before
 *  (story_interrupts(), PokemonBDSP_AutoStory_StoryInterrupts.h).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryExecutor_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryExecutor_H

#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RobustRoute.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_RoutePlanner.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"
#include "PokemonBDSP_AutoStory_StoryInterrupts.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Starter Pokemon options for BDSP story automation.
enum class StarterChoice{
    TURTWIG,
    CHIMCHAR,
    PIPLUP,
};

//  What a row does when one of its steps fails.
enum class StoryFailure{
    //  Save before the first step; on a failure reset to that save and run
    //  the row again from its first step.
    RELOAD_AND_RETRY,
    //  No save and no retry: the failure ends the program (00.00, before
    //  the game has anything worth saving).
    STOP,
};

//  A row of the story table: the shared checkpoint, and what a failure does.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    StoryFailure on_failure = StoryFailure::RELOAD_AND_RETRY;
    //  The map the save screen's card names where the row starts ("Route
    //  203"). The save at its start is refused anywhere else and the program
    //  stops: r76/r77 started 01.06 a walk away from its start and saved over
    //  the good save there. Empty for a row that does not save first.
    std::string saves_in;
};

//  What the story's steps act on.
struct StoryEnv{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& context;
    StarterChoice starter = StarterChoice::CHIMCHAR;
    std::string player_name;
};


//  The variant of a ROUTE step whose target is the next scene's trigger:
//  the scene starts there and stops the walk, as a wall would (00.04's
//  Mom's call on (6, 9)). It plans with the route costs.
inline constexpr std::string_view ROUTE_ONTO_TRIGGER = "trigger";

//  The variant of a ROUTE step that, once at its target, runs its one leg
//  ("legs") until a scene's dialogue starts. It plans with the route costs.
inline constexpr std::string_view ROUTE_INTO_SCENE = "into_scene";

//  The plan costs a ROUTE step's variant names: "", ROUTE_ONTO_TRIGGER and
//  ROUTE_INTO_SCENE the route costs, "town" town_plan_costs(), "robust" the planner's
//  defaults, "line" waypoint_plan_costs() (a stretch of a line a human
//  walked). Nothing for another name.
std::optional<RobustPlanCosts> story_route_costs(const std::string& variant);


//  What the console does around each story step: the step's log line, and
//  a failed step's "checkpoint_reattempt_loop" line and screenshot.
class ConsoleStepGuard : public Pokemon::StepGuard{
public:
    //  `checkpoint` names the row running now; both must outlive the guard.
    ConsoleStepGuard(VideoStream& stream, const std::string& checkpoint)
        : m_stream(stream)
        , m_checkpoint(checkpoint)
    {}
    //  The rows whose failure ends the program (StoryFailure::STOP).
    void set_stop_rows(std::set<std::string> rows){
        m_stop_rows = std::move(rows);
    }

    virtual void before_step(const Pokemon::StoryStep& step) override;
    virtual bool stops_on_failure(const Pokemon::StoryStep& step) const override;
    virtual void on_failure(const Pokemon::StoryStep& step, const OperationFailedException& error) override;
    virtual void on_interrupt(const Pokemon::StoryStep& step, const Pokemon::InterruptCheck& check) override;
    virtual void on_screen(const Pokemon::StoryStep& step, const Pokemon::ScreenSettle& screen) override;

private:
    VideoStream& m_stream;
    const std::string& m_checkpoint;
    std::map<std::string, size_t> m_failures;
    std::set<std::string> m_stop_rows;
    //  The row whose start screen is kept: a retry's start is kept again.
    std::string m_started;
};


//  Does BDSP's kinds of story step on the console: the story's action
//  table, its rows that need the console bound to the methods below.
class StoryExecutor : public Pokemon::TableExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);

    StoryEnv& env(){
        return m_env;
    }
    VideoStream& stream(){
        return m_env.env.console;
    }
    ProControllerContext& context(){
        return m_env.context;
    }
    //  The rows whose failure ends the program (StoryFailure::STOP): a
    //  failed step there throws on instead of failing the step.
    void set_stop_rows(std::set<std::string> rows){
        m_guard.set_stop_rows(std::move(rows));
    }
    //  A new attempt of a row: every trainer can see us again.
    virtual void begin_attempt(const Pokemon::StoryCheckpoint& data) override;

protected:
    //  The row running now.
    const std::string& checkpoint() const{
        return m_checkpoint;
    }
    //  The intro of a new game, from the title screen to the bedroom.
    Pokemon::Outcome new_game(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  verify_tutorial_settings(): fast text, Autosave off.
    Pokemon::Outcome options(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  A for YES, B for NO, once the question's box shows.
    Pokemon::Outcome answer(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  A trainer's text opens (or has) and ends in a battle, fought with the
    //  best move; one lost fails the step unless `may_lose`. Variant
    //  "doubles" for a tag battle.
    Pokemon::Outcome battle(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  `battles` wild battles in the grass we stand in, with the best move.
    Pokemon::Outcome train(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  In front of a Pokemon Center, its lit doorway on screen: in, the
    //  nurse, and out (heal_at_pokemon_center()).
    Pokemon::Outcome heal(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    Pokemon::Outcome save(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  The event its variant names (story_events()).
    Pokemon::Outcome game(Pokemon::RunState& state, const Pokemon::StoryStep& step);

private:
    StoryBoundActions console_actions();
    StoryInterruptActions console_interrupt_actions();

    StoryEnv& m_env;
    ConsoleStoryStepIO m_io;
    //  The steps that walk and talk, the table's rows for them:
    //  - ROUTE. Variant: the plan's costs ("" route, "town", "robust", "line"; "exit" is route's with "leave":
    //    a fight or a lost count ends in a hold to the room's door that way).
    //  - LEGS: each leg in D-pad taps, turning first. Variant "one_short_ok":
    //    a leg may end a tile short (a door lined up by sight after). Variant
    //    "to_wall": each leg walks until something stops it, for at most its
    //    tiles' time, as measured on the console; the pose is then unknown
    //    unless the step gives one. Variant "counted_wall": its one leg walks
    //    to the wall, waiting out anyone walking about in the way, and fails
    //    when the tiles walked are far off the map's from the step's start.
    //    Variant "measured": each leg in taps; the tiles of the step's start
    //    whose taps the map lets move as many as moved are where we are.
    //  - STEP: one D-pad step `direction`, turning first: onto a scene's
    //    trigger or into a door. Variant "to_banner": one tap into a cave
    //    mouth, then wait for pose_after's map's location banner. Variant
    //    "or_legs": into a door, and with no fade, the step's legs onto the
    //    door's tile and into it again.
    //  - TALK: A at what we face; a dialogue must open.
    //  - MASH: a dialogue opens (or has), then B through it to the overworld;
    //    variant "a": A through it, for lines that give an item.
    StoryWalkSteps m_walks;
    std::string m_checkpoint;
    std::map<std::string, size_t> m_run_counts;
    ConsoleStepGuard m_guard;
    //  The story's actions, its console rows bound to this executor.
    Pokemon::StoryActionTable m_table;
    //  What is cleared between steps, looked for on the screen.
    ConsoleScreenLook m_look;
    Pokemon::StoryInterrupts m_interrupts;
};


//  `rows` (kept alive by the caller) as one segment. A row that reloads on
//  failure saves first, unless `saved` says it has in this run: a retry
//  starts from that save. `executor` and `saved` must outlive the segment.
Pokemon::Segment make_story_segment(
    const std::vector<StoryCheckpoint>& rows, Pokemon::StoryStepExecutor& executor,
    std::set<std::string>& saved
);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
