/*  AutoStory Events
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The FRLG story as data for the shared story engine
 *  (Pokemon/Navigation/Pokemon_StoryEvents.h, Pokemon_StorySegments.h).
 *  A checkpoint is a list of StorySteps, each one kind of FRLG event (a
 *  route, a talk, text read to its end, a YES/NO, a battle, the nurse, the
 *  Mart); make_story_segment() turns the table into engine checkpoints whose
 *  events run them on the console. Routes plan from the pose the run
 *  carries and return where they leave the player; scenes that move the
 *  player say where in the table.
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
#include "Pokemon/Navigation/Pokemon_StoryEvents.h"
#include "Pokemon/Navigation/Pokemon_StorySegments.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Where the player stands and faces. An unknown facing (after a scene that
//  turns the player) is set before the next route by turning to a wall.
struct StoryPose{
    std::string map;
    TilePos tile;
    std::optional<Direction> facing;
};

enum class StoryStepKind{
    NEW_GAME,       //  title screen (no save) to the bedroom
    SET_OPTIONS,    //  text speed FAST, battle style SET
    ROUTE,          //  plan and walk to `target`
    LEGS,           //  walk `legs` as given, on one map
    STEP,           //  one step `direction`: onto a trigger, into a door
    TALK,           //  A at what the player faces
    ANSWER,         //  text with B to a YES/NO, then `yes`
    MASH,           //  text with B until the overworld is back
    BATTLE,         //  text with B into a battle, fight it, text to the overworld
    HEAL,           //  facing the nurse
    SHOP,           //  facing the Viridian Mart clerk: Potions and Antidotes
    READY,          //  before a big battle: items for status and HP, the nurse at `target` if they can't
};

//  A straight leg of a fixed walk.
struct StoryLeg{
    Direction direction = Direction::NORTH;
    int tiles = 0;
};

struct StoryStep{
    StoryStepKind kind = StoryStepKind::TALK;
    std::string name;
    std::string why;
    //  ROUTE: the goal, and the way to face there if set. READY: the nurse.
    StoryPose target;
    //  STEP
    Direction direction = Direction::NORTH;
    //  LEGS
    std::vector<StoryLeg> legs;
    //  ROUTE, LEGS: fight wild battles for EXP instead of running.
    bool fight_wild = false;
    //  ANSWER
    bool yes = true;
    //  BATTLE: the starter may faint without a blackout (the lab's rival).
    bool may_lose = false;
    //  Where a scene leaves the player.
    std::optional<StoryPose> pose_after;
};

struct StoryCheckpoint{
    std::string id;
    std::string title;
    //  Where its starting save leaves the player; none for the new game.
    std::optional<StoryPose> start;
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies;
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
    std::vector<StoryStep> steps;
};


//  The maps the story plans on: every map to Brock, with the objects the
//  game hides after the Hall of Fame removed, as the lab's frlg_route does.
const std::vector<MapData>& story_maps();

Pokemon::Pose to_pose(const StoryPose& pose);

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
};

//  The checkpoint `data` as events, saving at its end.
Pokemon::Checkpoint build_checkpoint(const StoryCheckpoint& data, StoryEnv& env);

//  `checkpoints` (kept alive by the caller) as a segment for run_segment().
Pokemon::Segment make_story_segment(const std::vector<StoryCheckpoint>& checkpoints, StoryEnv& env);



}
}
}
#endif
