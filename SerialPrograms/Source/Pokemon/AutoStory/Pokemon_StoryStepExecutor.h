/*  Story Step Executor
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One vocabulary of story steps for the tile games (FRLG, BDSP): a route,
 *  a fixed walk, a step onto a trigger, a talk, a YES/NO, a scene, a
 *  battle, the nurse, a shop, a save, and so on. A checkpoint is a list of
 *  such steps, plain data (Types/), so a game's story is a table its unit
 *  tests can plan without a console.
 *
 *  build_story_checkpoint() turns a table row into engine events
 *  (Pokemon_StoryEvents.h). Each event's action asks the game's
 *  StoryStepExecutor to do its step; a game implements the kinds it uses,
 *  and a step it has no way of doing fails by name (unsupported_step())
 *  instead of doing something else.
 *
 *  Steps never match dialogue or menu text: they look for screen state (a
 *  dialogue box, an arrow, a fade, a battle menu, colours, positions). The
 *  few reads that need text (item names in a shop or the Bag) take the
 *  game's language from the program's options (StoryRunOptions), and with
 *  no reader for it fall back to checks that need none (a row's position,
 *  digits, a changed screen), so a run never fails because its language
 *  is not supported.
 *
 *  Poses are the game's own (Pose, with a TilePose for these games): a
 *  checkpoint's start, a scene's end and a route's target are poses, and
 *  an executor that knows where a step left the player says so in its
 *  Outcome, which the engine writes into the run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStepExecutor_H
#define PokemonAutomation_Pokemon_StoryStepExecutor_H

#include <set>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon_StoryEvents.h"
#include "Pokemon_TilePose.h"
#include "Types/Pokemon_GridPose.h"
#include "Types/Pokemon_StoryCheckpoint.h"
#include "Types/Pokemon_StoryStep.h"
#include "Types/Pokemon_StoryStepKind.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How a game does its story steps: an interface with no data. run() is the
//  game's: it does the step's kind its own way (FRLG and BDSP bind kinds and
//  variants to actions in a table), and returns unsupported_step() for a
//  kind it has no way of doing.
class StoryStepExecutor{
public:
    virtual ~StoryStepExecutor() = default;

    //  A checkpoint is being built for a new attempt: build_story_checkpoint()
    //  calls this first, so a game can start the attempt afresh (trainers
    //  who can see us again, the row's own settings). Does nothing by default.
    virtual void begin_attempt(const StoryCheckpoint& data);

    //  Does `step`, from the run so far.
    virtual Outcome run(RunState& state, const StoryStep& step) = 0;

    //  Whether the condition named in a step's skip_if holds: by default,
    //  whether the run has the flag (set by an earlier step's action).
    virtual bool holds(std::string_view condition, RunState& state);
};

//  `pose` as the engine's Pose: a TilePose, TILE for one tile, SET for several.
Pose pose_of(const GridPose& pose);

//  "unsupported_step: TRAIN train_in_grass": what an executor without the kind returns.
Outcome unsupported_step(const StoryStep& step);


//  The step as an event that runs it with `executor`. Both must outlive
//  the event.
Event story_event(const StoryStep& step, StoryStepExecutor& executor);

//  The once_per_save steps done from the save the game is on that changed
//  nothing, as "<checkpoint>/<step>". The game keeps it across reloads and
//  empties it when it saves or loads another save.
using PreparedSteps = std::set<std::string>;

//  The checkpoint `data` as events: "from_save" first when it has a start
//  (the start pose, for a run that reloaded its save), then one event per
//  step. `data` and `executor` must outlive the checkpoint. Saving is the
//  game's: it sets the checkpoint's save, or adds a SAVE step.
//  With `prepared`, each once_per_save step is skipped while `prepared` has
//  it, and added to it when it succeeds changing nothing; `prepared` must
//  then outlive the checkpoint. Without, once_per_save steps run every time.
Checkpoint build_story_checkpoint(
    const StoryCheckpoint& data, StoryStepExecutor& executor, PreparedSteps* prepared = nullptr
);

//  Problems an author should fix before running: an empty id, unnamed or
//  repeated steps, a GAME step without an action, a built-in action's args
//  that don't fit its spec (a route without a target), empty legs.
std::vector<std::string> lint_story_checkpoint(const StoryCheckpoint& data);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
