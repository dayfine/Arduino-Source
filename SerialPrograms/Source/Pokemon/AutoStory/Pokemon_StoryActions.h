/*  Story Actions
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A game's story steps as a table of actions instead of one virtual per
 *  kind. An action is keyed by the step's kind and its action's name
 *  (StoryStep::action), so one table holds a game's
 *  usual way of doing a kind, its other ways, and the steps only it has.
 *  The console and a simulation bind the same rows to different actions.
 *
 *  TableExecutor runs a step through the table, inside the game's
 *  StepGuard: the log line, and a failed console step turned into a failed
 *  Outcome. Today each game writes that guard itself (BDSP's
 *  StoryExecutor::run(), FRLG's guarded(), PLZA's Executor::run()).
 *  With the game's interrupts set, it also clears what stopped the player
 *  between steps (Pokemon_StoryInterrupts.h).
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryActions_H
#define PokemonAutomation_Pokemon_StoryActions_H

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_StoryInterrupts.h"
#include "Pokemon_StoryStepExecutor.h"
#include "Types/Pokemon_StoryActionSpec.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Does `step`, from the run so far.
using StoryActionFunction = std::function<Outcome(const StoryStep& step, RunState& state)>;

//  A row of a StoryActionTable: what does a step, and what it needs.
struct StoryAction{
    StoryActionFunction act = {};
    //  Moves or reads the console. A binding without one (a simulation)
    //  can't run it.
    bool needs_console = false;
    //  The screen event the action ends on ("fade", "dialog"). Declared for
    //  the event-driven waits to come; nothing reads it yet.
    std::string waits_for = {};
    //  The args its steps may carry (StoryStep::args); a step with any
    //  other, or without a required one, fails lint_story_checkpoint().
    std::vector<StoryArgSpec> args = {};
};


//  A game's actions by step kind and action name.
class StoryActionTable{
public:
    //  The action for steps of `kind` whose action is named `name`. An
    //  empty `name` is the kind's default. A second action for the same
    //  key throws InternalProgramError.
    void add(StoryStepKind kind, std::string name, StoryAction action);
    //  The action for steps of any kind whose action is named `name`,
    //  which must not be empty.
    void add_any_kind(std::string name, StoryAction action);

    //  The action for a step of `kind` with an action named `name`: the
    //  one added for both, else the one for `name` of any kind, else the
    //  kind's default. nullptr if there is none.
    const StoryAction* find(StoryStepKind kind, std::string_view name) const;
    //  find() for the step's kind and its action's name.
    const StoryAction* find(const StoryStep& step) const;

private:
    using ByName = std::map<std::string, StoryAction, std::less<>>;
    std::map<StoryStepKind, ByName> m_by_kind;
    ByName m_any_kind;
};

//  The steps of `data` that `table` has no action for, as
//  "<checkpoint> step <n> <name>: no action for GAME 'bill'", or whose args
//  don't fit their action's, as "... missing argument \"budget\" for GAME 'mart'".
std::vector<std::string> lint_story_checkpoint(const StoryCheckpoint& data, const StoryActionTable& table);


//  What a game does around every step a TableExecutor runs. Does nothing
//  by default, and a failed step fails the step.
class StepGuard{
public:
    virtual ~StepGuard() = default;

    //  Before the step runs: e.g. the step's log line.
    virtual void before_step(const StoryStep& step);
    //  Whether a failed step stops the program, leaving the console on the
    //  failure, instead of failing the step for the segment to reload: the
    //  rows a developer is fixing.
    virtual bool stops_on_failure(const StoryStep& step) const;
    //  The step threw `error` and fails: e.g. log it and save a screenshot.
    virtual void on_failure(const StoryStep& step, const OperationFailedException& error);
    //  A look before the step, or after it failed, found an interrupt and
    //  cleared it or failed to (`check`): e.g. log it.
    virtual void on_interrupt(const StoryStep& step, const InterruptCheck& check);
    //  How the screen looked when the look before the step (or after it
    //  failed) ended, interrupt or not: e.g. log it, so a run log shows
    //  whether the scene before had ended before the step's first input.
    virtual void on_screen(const StoryStep& step, const ScreenSettle& screen);
};


//  What a look at the screen after a step failed found.
enum class AfterFailure{
    //  Nothing explains the failure: it is the step's own.
    KEEP,
    //  An interrupt explains it and is cleared: the step runs again.
    RETRY,
    //  The game is not running (the Switch's HOME menu): the failure is
    //  that, and the step is not run again.
    GAME_NOT_RUNNING,
};

//  Runs each step with the table's action for it, inside `guard`.
class TableExecutor : public StoryStepExecutor{
public:
    //  `table` and `guard` must outlive the executor.
    TableExecutor(const StoryActionTable& table, StepGuard& guard);

    //  The table's action for the step (run_action()); with none, the
    //  step is unsupported (unsupported_step()). An OperationFailedException
    //  from the action is the failed Outcome "operation_failed", unless the
    //  guard stops on it.
    //  With interrupts set, each step first clears what stopped the player
    //  since the step before, and a failed step that an interrupt explains
    //  runs again once it is cleared.
    virtual Outcome run(const StoryStep& step, RunState& state) override;

    //  The game's interrupts between steps, which must outlive the
    //  executor; none (the default) looks for none.
    void set_interrupts(const StoryInterrupts* interrupts){
        m_interrupts = interrupts;
    }

protected:
    //  The table's action for the step, or unsupported_step() with none:
    //  what run() runs once the screen is clear. A binding may wrap it, or
    //  do the kinds its table does not have yet.
    virtual Outcome run_action(const StoryStep& step, RunState& state);

private:
    std::optional<Outcome> clear_interrupts(const StoryStep& step, RunState& state, size_t& cleared);
    AfterFailure look_after_failure(const StoryStep& step, RunState& state, size_t& cleared);

    const StoryActionTable& m_table;
    StepGuard& m_guard;
    const StoryInterrupts* m_interrupts = nullptr;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
