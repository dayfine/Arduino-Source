/*  AutoStory Poses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the story stands and how fast it moves: its poses, the maps it
 *  plans on, and the run variant.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPoses_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPoses_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
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
//  has the Running Shoes (00.10 on).
inline const std::string RUN_VARIANT = "run";

//  The GAME step that closes the Trainer Card SID Helper leaves open.
inline const std::string CLOSE_TRAINER_CARD = "close_trainer_card";

//  How `step`, a ROUTE or LEGS, moves the player: RUN_VARIANT runs.
MoveSpeed move_speed(const StoryStep& step);
//  The maps the story plans on: every map to Brock, with the objects the
//  game hides after the Hall of Fame removed, as the lab's frlg_route does.
const std::vector<MapData>& story_maps();

//  Converts a StoryPose to a general navigation Pose.
Pokemon::Pose to_pose(const StoryPose& pose);
//  `pose` as a StoryPose: nothing unless it is on one tile.
std::optional<StoryPose> story_pose(const Pokemon::Pose& pose);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
