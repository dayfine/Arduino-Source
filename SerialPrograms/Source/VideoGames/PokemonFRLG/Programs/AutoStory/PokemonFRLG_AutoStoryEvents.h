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
#include "PokemonFRLG_AutoStoryStartFrames.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
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
//  has the Running Shoes (03.03 on).
inline const std::string RUN_VARIANT = "run";

//  The variant of a STEP into a trainer's line: a tile at a time, at most
//  TRAINER_LINE_TILES, until the trainer sees the player
//  (step_until_seen()). The battle after it starts on the trainer's row
//  whether the walk before ended on its tile or a tile short.
inline const std::string TRAINER_LINE_VARIANT = "trainer_line";
//  Maximum tiles to step into a trainer's line of sight.
inline constexpr int TRAINER_LINE_TILES = 3;

//  The variant of a STEP into the line of a trainer who looks around
//  (MOVEMENT_TYPE_LOOK_AROUND): one tile, then a wait of up to
//  TRAINER_WAIT_WATCH for the trainer to turn to the player. Indoors, where
//  the player can't run, such a trainer turns a random way every 32 to 128
//  frames (pokefirered event_object_movement.c, MovementType_LookAround;
//  it turns to the player only when the player runs), so they face the
//  player within 30 s all but about 1 time in 500; and the game checks
//  their sight every frame (ProcessPlayerFieldInput()), not only after a
//  step. Stepped into from outside their sight onto the second tile of
//  their line, the player is always seen there, and the trainer walks up
//  one tile.
inline const std::string TRAINER_WAIT_VARIANT = "trainer_wait";

//  The GAME step that cuts the tree the player faces, `direction`
//  (pokefirered data/scripts/field_moves.inc, EventScript_CutTree): A,
//  "This tree looks like it can be CUT down! Would you like to CUT it?"
//  YES, "<MON> used CUT!", the tree falls; then a step onto its tile. With
//  no Pokemon that knows Cut there is no YES/NO, and the step fails. The
//  tree grows back when the player leaves the map (its hide flag is a
//  FLAG_TEMP), so the walk back cuts it again.
inline const std::string CUT_TREE_VARIANT = "cut_tree";

//  The GAME step that closes the Trainer Card SID Helper leaves open.
inline const std::string CLOSE_TRAINER_CARD = "close_trainer_card";

//  The GAME step that leaves the gift's summary Gift RNG leaves open for
//  the overworld.
inline const std::string LEAVE_GIFT_SUMMARY = "leave_gift_summary";

//  How `step`, a ROUTE or LEGS, moves the player: RUN_VARIANT runs,
//  BIKE_VARIANT rides.
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
    //  with `rows` empty, the run started at 04.03 and takes Gift RNG from
    //  the save beside the salesman, where the row before `first` ends.
    StoryHandoff stop = StoryHandoff::NONE;
};

//  The rows of `checkpoints` from `first` to `last`, both inclusive, as a
//  run with `handoffs` takes them: live-start rows only when the run
//  starts there (04.03 with Gift RNG's handoff on starts at the handoff
//  instead), a run that reaches a handoff that is on stops for it, and
//  a STARTER_RNG row whose handoff is off is joined with the row after it,
//  under its own id.
StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
);


//  The maps the story plans on: every map to Route 4's Pokemon Center, with
//  the objects the game hides after the Hall of Fame removed, as the lab's
//  frlg_route does, and those a new game hides until their scene (the
//  S.S. Anne's rival on the Captain's door).
const std::vector<MapData>& story_maps();
//  default_route_costs() without Surf, which the story hasn't got yet:
//  routes stay on land, even where water is the shorter way (Vermilion
//  City's harbor).
const RouteCosts& story_route_costs();

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

//  Adds to `checkpoint`, built from a row that starts at `start`, the start
//  check: the save it starts from, as `read` reads it, must stand there
//  (start_pose_mismatch()), or the checkpoint stops before its first event,
//  "start_failed". A save that can't be read passes. `log` gets a
//  mismatch, or that nothing was read. A row with no start (a new game)
//  gets none. (add_start_check() with a reader of the tile.)
void add_start_pose_check(
    Pokemon::Checkpoint& checkpoint, const std::optional<Pokemon::Pose>& start, SavePoseReader read, StoryLog log
);

//  Where a GRIND or capture `step` that paces (pace = ...) leaves the
//  player who started it on `patch`: the patch's tile or the one its pace
//  leads to, as the last battle came, facing unknown. `patch` for any
//  other step, or a `patch` not on one tile.
Pokemon::Pose paced_pose(const Pokemon::Pose& patch, const StoryStep& step);

//  A ROUTE's legs from `pose` to `goal`, on story_route_costs(): from
//  one tile, as planned; from a set of tiles (paced_pose()), first the
//  holds to walls that bring them all to one tile (holds_to_one_tile()),
//  then the route from there.
//  Nothing if either can't be planned.
std::optional<std::vector<RouteStep>> plan_story_route(
    const std::vector<MapData>& maps, const Pokemon::Pose& pose,
    std::optional<Direction> facing, const RouteGoal& goal
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
    //  The frames of where each checkpoint starts, for the start check: a
    //  save at a checkpoint's end keeps one for the next checkpoint.
    StartFrameStore start_frames = {};
    //  The next start check keeps the screen as its checkpoint's start
    //  frame and passes: the user saved there by hand.
    bool keep_start_frame = false;
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

//  Does FRLG's kinds of story step on the console, each with its row of
//  story_action_rows() (PokemonFRLG_AutoStoryStepTable.h). A console step
//  that fails is a failed Outcome, which the segment answers by reloading.
//  Before each step it clears what came up since the step before
//  (step_interrupts(), PokemonFRLG_AutoStoryStepInterrupts.h).
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
    //  A failed step fails the step; an interrupt between steps is logged.
    InterruptLogGuard m_guard;
    Pokemon::StoryActionTable m_table;
    Pokemon::TableExecutor m_executor;
    //  What is cleared between steps, looked for on the screen.
    ConsoleScreenLook m_look;
    Pokemon::StoryInterrupts m_interrupts;
};

//  The checkpoint `data` as events, saving at its end.
Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryExecutor& executor);

//  `checkpoints` (kept alive by the caller) as a segment for run_segment().
Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryExecutor& executor);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
