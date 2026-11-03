/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG story as data for the shared story engine
 *  (Pokemon/AutoStory/Pokemon_StoryEvents.h, Pokemon_StorySegments.h).
 *  A checkpoint is a list of the shared StorySteps
 *  (Pokemon/AutoStory/Pokemon_StorySteps.h), each one kind of event (a
 *  route, a talk, text read to its end, a YES/NO, a battle, the nurse, the
 *  Mart); StoryExecutor does each kind on the console, and
 *  make_story_segment() turns the table into engine checkpoints. Routes
 *  plan from the pose the run carries and return where they leave the
 *  player; scenes that move the player say where in the table.
 *
 *  The table itself is in PokemonFRLG_AutoStoryTable.h; it is plain data,
 *  so its routes are planned in unit tests without a console.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryEvents_H
#define PokemonAutomation_PokemonFRLG_AutoStoryEvents_H

#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


using Pokemon::StoryLeg;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;

//  Where the player stands and faces: the one-tile poses of FRLG's table
//  and routes. An unknown facing (after a scene that turns the player) is
//  set before the next route by turning to a wall.
struct StoryPose{
    std::string map;
    TilePos tile;
    std::optional<Direction> facing;
};

//  The variant of a ROUTE or LEGS step that runs, with B held: the player
//  has the Running Shoes (00.10 on).
inline const std::string RUN_VARIANT = "run";

//  The variant of a STEP into a trainer's line: a tile at a time, at most
//  TRAINER_LINE_TILES, until the trainer sees the player
//  (step_until_seen()). The battle after it starts on the trainer's row
//  whether the walk before ended on its tile or a tile short.
inline const std::string TRAINER_LINE_VARIANT = "trainer_line";
inline constexpr int TRAINER_LINE_TILES = 3;

//  The GAME step that closes the Trainer Card SID Helper leaves open.
inline const std::string CLOSE_TRAINER_CARD = "close_trainer_card";

//  The GAME step that leaves the gift's summary Gift RNG leaves open for
//  the overworld.
inline const std::string LEAVE_GIFT_SUMMARY = "leave_gift_summary";

//  How `step`, a ROUTE or LEGS, moves the player: RUN_VARIANT runs.
MoveSpeed move_speed(const StoryStep& step);


//  An upstream RNG program the user may run part-way through the story.
//  Each is off unless its option is on.
enum class StoryHandoff{
    NONE,
    //  The checkpoint's new game stops on the player's keyboard, the name
    //  typed and OK not pressed, which is where SID Helper starts.
    SID_HELPER,
    //  The checkpoint saves south of the table, facing the starter's ball,
    //  which is where Starter RNG starts. With the handoff off, it runs on
    //  into the next checkpoint as one, as the story did before the split.
    STARTER_RNG,
    //  The checkpoint saves beside the Magikarp salesman, facing him, which
    //  is where Gift RNG starts.
    GIFT_RNG,
};

//  A shared checkpoint with what FRLG adds to it.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies;
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
    //  Starts from the game as another program left it, loading no save:
    //  the way back into the story after an RNG program. A run that starts
    //  before it passes over it.
    bool live_start = false;
    //  The RNG program the user may run at this checkpoint.
    StoryHandoff handoff = StoryHandoff::NONE;
};

//  The handoffs a run stops for (AutoStory's options).
struct StoryHandoffs{
    bool sid_helper = false;
    bool starter_rng = false;
    bool gift_rng = false;
};

//  What a run from table row `first` to `last` does.
struct StoryRunPlan{
    //  The checkpoints to run, in order.
    std::vector<StoryCheckpoint> rows;
    //  NONE: the run ends after `rows`. SID_HELPER: `rows` is empty; the run
    //  plays the new game to the player's keyboard and stops there.
    //  STARTER_RNG: the run stops after `rows`, the last of which saves at
    //  the starter's ball. GIFT_RNG: likewise, beside the Magikarp salesman;
    //  with `rows` empty, the run started at 00.12b and takes Gift RNG from
    //  the save beside the salesman, where the row before `first` ends.
    StoryHandoff stop = StoryHandoff::NONE;
};

//  The rows of `checkpoints` from `first` to `last`, both inclusive, as a
//  run with `handoffs` takes them: live-start rows only when the run
//  starts there (00.12b with Gift RNG's handoff on starts at the handoff
//  instead), a run that reaches a handoff that is on stops for it, and
//  a STARTER_RNG row whose handoff is off is joined with the row after it,
//  under its own id.
StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
);


//  The maps the story plans on: every map to Route 4's Pokemon Center, with
//  the objects the game hides after the Hall of Fame removed, as the lab's
//  frlg_route does.
const std::vector<MapData>& story_maps();

Pokemon::Pose to_pose(const StoryPose& pose);
//  `pose` as a StoryPose: nothing unless it is on one tile.
std::optional<StoryPose> story_pose(const Pokemon::Pose& pose);

//  The place the save card names for a save on `map` (see
//  read_save_card_location()), or empty for a map whose place isn't known.
std::string save_card_place(const std::string& map);

//  Why a save whose card names `place` can't start a checkpoint that starts
//  on `start_map`, or empty if it can: the places match, the card couldn't
//  be read (empty `place`), or the map has no known place.
std::string save_place_error(const std::string& start_map, const std::string& place);

//  `legs` from `start` as route steps for the walker, each checked against
//  the map: nothing if a step is blocked or leaves the map. A leg ends "to
//  wall" if the next step its way is blocked.
std::optional<std::vector<RouteStep>> fixed_route(
    const std::vector<MapData>& maps, const MapPosition& start, const std::vector<StoryLeg>& legs
);

//  A way the player at `tile` can turn to without stepping: a wall there,
//  with no one standing in it. West and east first: walking north or south
//  into a sign would read it.
std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile);

//  `items` as orders at the Viridian Mart before the Boulder Badge: the
//  rows of POKE BALL, POTION, ANTIDOTE, PARLYZ HEAL. Nothing if it does
//  not sell one of them.
std::optional<std::vector<MartOrder>> viridian_mart_orders(const std::vector<Pokemon::StoryItem>& items);


//  What the story's events act on. `supplies` are what the bag holds now,
//  `saved_supplies` what the last save holds.
struct StoryEnv{
    ConsoleHandle& console;
    ProControllerContext& context;
    Supplies supplies;
    Supplies saved_supplies;
    //  Whether the game has a save to reload.
    bool has_save = false;
    StoryChecks checks = StoryChecks::THOROUGH;
    //  The game's language, for reading item names. In one the readers have
    //  no names for, the checks fall back to positions and counts.
    Language language = Language::English;
    //  The name the new game's intro types for the player; empty keeps the
    //  game's default name.
    std::string player_name = {};
    //  The map the checkpoint under way starts on, where its save is: a
    //  reload checks the save card against it.
    std::string start_map = {};
    //  That checkpoint's START menu: a reload reads the bag again with it.
    StartMenu start_menu = StartMenu::BEFORE_POKEMON;
    //  The once_per_save steps done from the last save that changed
    //  nothing: a retry after a reload skips them. Emptied by a save.
    Pokemon::PreparedSteps prepared = {};
    //  The last save's Items pocket was read in full (read_bag_supplies()):
    //  a reload restores `saved_supplies` without reading it again.
    bool bag_read = false;
};

//  After a save is loaded for a checkpoint that starts on `start_map`: the
//  save card's place, read from the START menu, against it, at every checks
//  level. Empty if the save is there (or the card can't be read), else why
//  not.
std::string check_save_place(StoryEnv& env, const std::string& start_map);

//  After a save is loaded, with START's menu `menu`: the lead's moves as its
//  summary shows them, which the battles then start from in place of the
//  row's (in env's supplies and the ones a reload restores). The row's are
//  kept if no move can be read.
void read_lead_moves(StoryEnv& env, StartMenu menu);

//  After a save is loaded, with START's menu `menu`: the Items pocket's
//  counts as read (set_from_loaded_bag()), in place of the row's, which
//  are kept if nothing can be read. Only with the full menu: the rows
//  before it carry the items they have. Sets `bag_read` when every count
//  was read.
void read_bag_supplies(StoryEnv& env, StartMenu menu);

//  Does FRLG's kinds of story step on the console. A console step that
//  fails is a failed Outcome, which the segment answers by reloading.
class StoryExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);

    StoryEnv& env(){
        return m_env;
    }
    //  The START menu SET_OPTIONS leaves: the checkpoint's being built.
    void set_menu(StartMenu menu){
        m_menu = menu;
    }

    virtual Pokemon::Outcome run(Pokemon::RunState& state, const StoryStep& step) override;

protected:
    virtual Pokemon::Outcome new_game(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome options(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome route(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome legs(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome step(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome talk(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome answer(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome mash(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome battle(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome heal(Pokemon::RunState& state, const StoryStep& step) override;
    //  The Viridian Mart, before the Boulder Badge: Potions, Antidotes and Parlyz Heals.
    virtual Pokemon::Outcome shop(Pokemon::RunState& state, const StoryStep& step) override;
    virtual Pokemon::Outcome ready(Pokemon::RunState& state, const StoryStep& step) override;
    //  CLOSE_TRAINER_CARD: back from SID Helper (close_trainer_card()).
    //  LEAVE_GIFT_SUMMARY: back from Gift RNG (leave_gift_summary()).
    //  A capture or a PC deposit or withdrawal (capture_step(), pc_step()
    //  in PokemonFRLG_AutoStoryCapture.h); a shiny met in a capture stops
    //  the program without reloading. A MART, TEACH, LEVEL, GRIND or
    //  fossil CHOICE (PokemonFRLG_AutoStoryAbilities.h); a GRIND heals by
    //  routing to its target, the nurse, and back.
    virtual Pokemon::Outcome game(Pokemon::RunState& state, const StoryStep& step) override;

private:
    StoryEnv& m_env;
    StartMenu m_menu = StartMenu::FULL;
};

//  The checkpoint `data` as events, saving at its end.
Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor);

//  `checkpoints` (kept alive by the caller) as a segment for run_segment().
Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryExecutor& executor);



}
}
}
#endif
