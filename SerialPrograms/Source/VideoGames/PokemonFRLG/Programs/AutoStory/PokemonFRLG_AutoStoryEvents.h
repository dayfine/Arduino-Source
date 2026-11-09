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

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StorySteps.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryStepTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Story leg definition imported from Pokemon AutoStory.
using Pokemon::StoryLeg;
//  Story step definition imported from Pokemon AutoStory.
using Pokemon::StoryStep;
//  Story step kind classification imported from Pokemon AutoStory.
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

//  How `step`, a ROUTE or LEGS, moves the player: RUN_VARIANT runs.
MoveSpeed move_speed(const StoryStep& step);


//  A shared checkpoint with what FRLG adds to it.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies;
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
};


//  The maps the story plans on: every map to Brock, with the objects the
//  game hides after the Hall of Fame removed, as the lab's frlg_route does.
const std::vector<MapData>& story_maps();

//  Converts a StoryPose to a general navigation Pose.
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

//  Why a save standing at `stands` can't start a checkpoint that starts at
//  `start`, or empty if it can: the same map and tile, and the same facing
//  when both are known.
std::string start_pose_mismatch(const Pokemon::Pose& start, const StoryPose& stands);

//  Where the save just loaded stands, as the game reads it: nothing when it
//  can't be read.
using SavePoseReader = std::function<std::optional<StoryPose>()>;
//  Where the start check says what it found.
using StoryLog = std::function<void(const std::string& line)>;

//  The checkpoint `data` as events (Pokemon::build_story_checkpoint()), and
//  for a row with a start pose, the start check: the save it starts from,
//  as `read` reads it, must stand there (start_pose_mismatch()), or the
//  checkpoint stops before its first event, "start_failed". A save that
//  can't be read passes. `log` gets a mismatch, or that nothing was read.
Pokemon::Checkpoint build_checked_checkpoint(
    const StoryCheckpoint& data, Pokemon::StoryStepExecutor& executor, SavePoseReader read, StoryLog log
);

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
    //  The map the checkpoint under way starts on, where its save is: a
    //  reload checks the save card against it.
    std::string start_map = {};
    //  That checkpoint's START menu: a reload reads the bag again with it.
    StartMenu start_menu = StartMenu::BEFORE_POKEMON;
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
//  before it carry the items they have.
void read_bag_supplies(StoryEnv& env, StartMenu menu);

//  Does FRLG's kinds of story step on the console, each with its row of
//  story_action_rows() (PokemonFRLG_AutoStoryStepTable.h). A console step
//  that fails is a failed Outcome, which the segment answers by reloading.
class StoryExecutor : public Pokemon::StoryStepExecutor{
public:
    explicit StoryExecutor(StoryEnv& env);
    //  Its table's actions point at it.
    StoryExecutor(const StoryExecutor&) = delete;
    StoryExecutor& operator=(const StoryExecutor&) = delete;

    StoryEnv& env(){
        return m_env;
    }
    //  The START menu SET_OPTIONS leaves: the checkpoint's being built.
    void set_menu(StartMenu menu){
        m_menu = menu;
    }

    //  The step's action from the table.
    virtual Pokemon::Outcome run(Pokemon::RunState& state, const StoryStep& step) override;

private:
    //  How the console does `id`.
    Pokemon::StoryAction console_action(StoryActionId id);

    StoryEnv& m_env;
    StartMenu m_menu = StartMenu::FULL;
    //  FRLG does nothing around a step: a failed one fails the step.
    Pokemon::StepGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::TableExecutor m_executor;
};

//  The checkpoint `data` as events, saving at its end.
Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor);

//  `checkpoints` (kept alive by the caller) as a segment for run_segment().
Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryExecutor& executor);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
