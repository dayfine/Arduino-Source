/*  BDSP AutoStory - Console story
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's story steps on the console: the story's actions (story_game(),
 *  PokemonBDSP_AutoStory_StoryActions.h) bound to the console and run by the
 *  shared story engine's step runner. A route is planned on the joined maps
 *  that hold its start and its target (story_plan_world()), from the tiles
 *  the run's pose holds, and walked by the walker, which fights the
 *  trainers who see us and runs from wild battles. A step whose variant
 *  names one of the story's events (story_events(),
 *  PokemonBDSP_AutoStory_Events.h) runs that event, whatever its kind: the
 *  scripted scenes and the console-measured walks of segment 01. The
 *  simulation binds the same actions.
 *
 *  Every step runs inside a guard (ConsoleStepGuard) that turns a failed
 *  console step into a failed Outcome, with a screenshot and the
 *  "checkpoint_reattempt_loop: attempt N failed" line the lab's run watcher
 *  reads; the segment then reloads the row's save and runs it again.
 *  Before each step it clears what came up since the step before
 *  (story_interrupts(), PokemonBDSP_AutoStory_StoryInterrupts.h).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_ConsoleStory_H
#define PokemonAutomation_PokemonBDSP_AutoStory_ConsoleStory_H

#include <map>
#include <set>
#include <string>
#include <utility>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_SingleSwitchProgram.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryCheckpoint.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryStep.h"
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
};


//  Does BDSP's kinds of story step on the console: the story's actions on
//  the engine (StoryEngineSteps), those that need the console bound to the
//  methods below.
class ConsoleStory : public Pokemon::TableExecutor{
public:
    explicit ConsoleStory(StoryEnv& env);

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
    //  The game; a row's start save (SAVES_IN_ARG) only where the card names its map.
    Pokemon::Outcome save(Pokemon::RunState& state, const Pokemon::StoryStep& step);
    //  The engine's action for the step.
    virtual Pokemon::Outcome run_action(const Pokemon::StoryStep& step, Pokemon::RunState& state) override;

private:
    StoryBoundActions console_actions();
    StoryInterruptActions console_interrupt_actions();

    StoryEnv& m_env;
    ConsoleStoryStepIO m_io;
    //  The steps that walk and talk, the table's rows for them:
    //  - ROUTE. Variant: the plan's costs ("" route, "town", "robust", "line").
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
    ConsoleStepGuard m_guard;
    //  TableExecutor's table: empty, as run_action() runs the engine's actions.
    Pokemon::StoryActionTable m_no_table;
    StoryLogBackend m_backend;
    //  The story's actions, those that need the console bound to this.
    StoryEngineSteps m_steps;
    //  What is cleared between steps, looked for on the screen.
    ConsoleScreenLook m_look;
    Pokemon::StoryInterrupts m_interrupts;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
