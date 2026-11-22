/*  BDSP AutoStory - Console story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's story steps on the console, compiled into #648 Pokemon::StoryCheckpoints
 *  for Pokemon::run_checkpoints(). Walking steps (route, legs, step, talk, scene)
 *  run through StoryWalkSteps on ConsoleStoryStepIO; console-only actions and
 *  story_events() run directly on the Switch.
 *
 *  Every step runs inside ConsoleStepGuard, which logs the step, clears
 *  between-step interrupts (story_interrupts()), turns a recoverable
 *  OperationFailedException into a StoryFailure with a screenshot and the
 *  "checkpoint_reattempt_loop: attempt N failed" line the lab watcher reads,
 *  and marks stop rows with StoryFailure::repeats = true (S15).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_ConsoleStory_H
#define PokemonAutomation_PokemonBDSP_AutoStory_ConsoleStory_H

#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "PokemonBDSP_AutoStory_StoryActions.h"
#include "PokemonBDSP_AutoStory_StoryCheckpoint.h"
#include "PokemonBDSP_AutoStory_StoryInterrupts.h"
#include "PokemonBDSP_AutoStory_StoryStepIO.h"
#include "PokemonBDSP_AutoStory_StoryWalkSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What the console does around each story step: the step's log line, and
//  a failed step's "checkpoint_reattempt_loop" line and screenshot.
class ConsoleStepGuard{
public:
    //  `checkpoint` names the row running now; both must outlive the guard.
    ConsoleStepGuard(VideoStream& stream, const std::string& checkpoint)
        : m_stream(stream)
        , m_checkpoint(checkpoint)
    {}
    //  The rows whose failure ends the program (StoryFailurePolicy::STOP).
    void set_stop_rows(std::set<std::string> rows){
        m_stop_rows = std::move(rows);
    }

    void before_step(const BdspStep& step);
    bool stops_on_failure() const;
    void on_failure(const BdspStep& step, const std::string& message);
    void on_interrupt(const BdspStep& step, const Pokemon::InterruptCheck& check);
    void on_screen(const BdspStep& step, const Pokemon::ScreenSettle& screen);

private:
    VideoStream& m_stream;
    const std::string& m_checkpoint;
    std::map<std::string, size_t> m_failures;
    std::set<std::string> m_stop_rows;
};


//  BDSP's story actions bound to a console, producing #648 engine checkpoints.
class ConsoleStory{
public:
    explicit ConsoleStory(StoryEnv& env);

    BdspLogBackend& backend(){
        return m_backend;
    }
    BdspRunState& run_state(){
        return m_run_state;
    }

    //  The rows whose failure ends the program (StoryFailurePolicy::STOP):
    //  a failed step there stops immediately without reloading.
    void set_stop_rows(std::set<std::string> rows){
        m_guard.set_stop_rows(std::move(rows));
    }

    //  Compiles `rows` into #648 Pokemon::StoryCheckpoints bound to this console.
    std::vector<Pokemon::StoryCheckpoint> make_checkpoints(std::span<const StoryCheckpoint> rows);

    //  Saves the game at the end of a checkpoint (E9). When `saves_in` is
    //  non-empty, checks the save card for that map and throws
    //  FatalProgramException if the card names another map.
    std::optional<Pokemon::StoryFailure> save_checkpoint(const std::string& saves_in);

private:
    VideoStream& stream(){
        return m_env.env.console;
    }
    ProControllerContext& context(){
        return m_env.context;
    }
    std::optional<Pokemon::StoryFailure> run_wrapped_step(
        const BdspStep& step,
        BdspRunState& state,
        const std::function<std::optional<Pokemon::StoryFailure>()>& run_action
    );
    std::optional<Pokemon::StoryFailure> new_game(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> options(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> answer(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> battle(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> train(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> heal(BdspRunState& state, const BdspStep& step);
    std::optional<Pokemon::StoryFailure> save(BdspRunState& state, const BdspStep& step);

    BdspStoryHandlers console_handlers();
    StoryInterruptActions console_interrupt_actions();

    StoryEnv& m_env;
    ConsoleStoryStepIO m_io;
    StoryWalkSteps m_walks;
    BdspRunState m_run_state;
    std::string m_checkpoint;
    ConsoleStepGuard m_guard;
    BdspLogBackend m_backend;
    ConsoleScreenLook m_look;
    Pokemon::StoryInterrupts m_interrupts;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
