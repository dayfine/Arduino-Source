/*  Story Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  One vocabulary of story steps for the tile games (FRLG, BDSP): a route,
 *  a fixed walk, a step onto a trigger, a talk, a YES/NO, a scene, a
 *  battle, the nurse, a shop, a save, and so on. A checkpoint is a list of
 *  such steps, plain data, so a game's story is a table its unit tests can
 *  plan without a console.
 *
 *  build_story_checkpoint() turns a table row into engine events
 *  (Pokemon_StoryEvents.h). Each event's action asks the game's
 *  StoryStepExecutor to do its step: one virtual per kind, so a game
 *  implements the kinds it uses, and a step the game has no way of doing
 *  fails by name instead of doing something else.
 *
 *  Poses are the game's own (Pose, with a TilePose for these games): a
 *  checkpoint's start, a scene's end and a route's target are poses, and
 *  an executor that knows where a step left the player says so in its
 *  Outcome, which the engine writes into the run.
 *
 */

#ifndef PokemonAutomation_Pokemon_StorySteps_H
#define PokemonAutomation_Pokemon_StorySteps_H

#include <cstddef>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon_Grid.h"
#include "Pokemon_StoryEvents.h"
#include "Pokemon_TilePose.h"

namespace PokemonAutomation{
namespace Pokemon{


enum class StoryStepKind{
    NEW_GAME,   //  title screen (no save) to the first room
    OPTIONS,    //  the game's options as the story needs them
    ROUTE,      //  plan and walk to `target`
    LEGS,       //  walk `legs` as given
    STEP,       //  one step `direction`: onto a trigger, into a door
    TALK,       //  A at what the player faces
    ANSWER,     //  text to a YES/NO, then `yes`
    MASH,       //  text until the overworld is back: a scene
    BATTLE,     //  text into a battle, fight it, text to the overworld
    TRAIN,      //  fight `battles` wild battles where the player stands
    HEAL,       //  the nurse, from in front of her or of the Center's door
    SHOP,       //  `items` from a shop
    SAVE,       //  save the game where the player stands
    READY,      //  before a big battle: items, and the nurse at `target` if they can't
    GAME,       //  a step only this game has, named by `variant`
};
const char* story_step_kind_name(StoryStepKind kind);


//  A straight leg of a fixed walk.
struct StoryLeg{
    Direction direction = Direction::NORTH;
    int tiles = 0;
};

//  An item and how many to get.
struct StoryItem{
    std::string item;
    size_t quantity = 0;
};

struct StoryStep{
    StoryStepKind kind = StoryStepKind::TALK;
    std::string name;
    std::string why;

    //  ROUTE: where to go; with several tiles, any of them. READY: the nurse.
    std::optional<Pose> target;
    //  STEP
    Direction direction = Direction::NORTH;
    //  LEGS
    std::vector<StoryLeg> legs;
    //  ROUTE, LEGS: fight wild battles on the way instead of running.
    bool fight_wild = false;
    //  ANSWER
    bool yes = true;
    //  BATTLE: losing goes on (a rival's first battle).
    bool may_lose = false;
    //  TRAIN
    size_t battles = 0;
    //  SHOP
    std::vector<StoryItem> items;
    //  How this game does the kind, when it has more than one way: a
    //  route's plan costs, a door lined up by sight, the Mart lady instead
    //  of the clerk. Empty is the game's usual way. GAME: the step's name
    //  in the game's executor.
    std::string variant;

    //  Skipped when all of these hold (and there is at least one): names
    //  the game's executor answers (StoryStepExecutor::holds()), run flags
    //  unless the game reads them off the screen.
    std::vector<std::string> skip_if;
    //  Where the step leaves the player, when its action does not say: the
    //  end of a scene that moves the player.
    std::optional<Pose> pose_after;
};

struct StoryCheckpoint{
    std::string id;
    std::string title;
    //  Where its starting save leaves the player; none for a new game.
    std::optional<Pose> start;
    std::vector<StoryStep> steps;
};


//  How a game does each kind of step. Every kind fails as unsupported
//  unless the game overrides it.
class StoryStepExecutor{
public:
    virtual ~StoryStepExecutor() = default;

    //  Runs `step`: dispatch_story_step() by default. A game overrides it
    //  to wrap every step, e.g. to turn a failed console step into a failed
    //  Outcome.
    virtual Outcome run(RunState& state, const StoryStep& step);

    //  Whether the condition named in a step's skip_if holds: by default,
    //  whether the run has the flag (set by an earlier step's action).
    virtual bool holds(const std::string& condition, RunState& state);

    virtual Outcome new_game(RunState& state, const StoryStep& step);
    virtual Outcome options(RunState& state, const StoryStep& step);
    virtual Outcome route(RunState& state, const StoryStep& step);
    virtual Outcome legs(RunState& state, const StoryStep& step);
    virtual Outcome step(RunState& state, const StoryStep& step);
    virtual Outcome talk(RunState& state, const StoryStep& step);
    virtual Outcome answer(RunState& state, const StoryStep& step);
    virtual Outcome mash(RunState& state, const StoryStep& step);
    virtual Outcome battle(RunState& state, const StoryStep& step);
    virtual Outcome train(RunState& state, const StoryStep& step);
    virtual Outcome heal(RunState& state, const StoryStep& step);
    virtual Outcome shop(RunState& state, const StoryStep& step);
    virtual Outcome save(RunState& state, const StoryStep& step);
    virtual Outcome ready(RunState& state, const StoryStep& step);
    virtual Outcome game(RunState& state, const StoryStep& step);
};

//  Calls the executor's method for the step's kind.
Outcome dispatch_story_step(StoryStepExecutor& executor, RunState& state, const StoryStep& step);

//  "unsupported_step: TRAIN train_in_grass": what an executor without the kind returns.
Outcome unsupported_step(const StoryStep& step);


//  The step as an event that runs it with `executor`. Both must outlive
//  the event.
Event story_event(const StoryStep& step, StoryStepExecutor& executor);

//  The checkpoint `data` as events: "from_save" first when it has a start
//  (the start pose, for a run that reloaded its save), then one event per
//  step. `data` and `executor` must outlive the checkpoint. Saving is the
//  game's: it sets the checkpoint's save, or adds a SAVE step.
Checkpoint build_story_checkpoint(const StoryCheckpoint& data, StoryStepExecutor& executor);

//  Problems an author should fix before running: an empty id, unnamed or
//  repeated steps, a route or READY without a target, empty legs, a GAME
//  step without a variant, TRAIN without battles, SHOP without items.
std::vector<std::string> lint_story_checkpoint(const StoryCheckpoint& data);


//  A pose on one of `tiles` of `map`, facing `facing` if set: TILE for one
//  tile, SET for several, UNKNOWN for none.
Pose tile_pose(std::string map, std::vector<TilePos> tiles, std::optional<Direction> facing = std::nullopt);

//  The facing of a tile pose; nothing for another pose or none.
std::optional<Direction> facing_of(const Pose& pose);



}
}
#endif
