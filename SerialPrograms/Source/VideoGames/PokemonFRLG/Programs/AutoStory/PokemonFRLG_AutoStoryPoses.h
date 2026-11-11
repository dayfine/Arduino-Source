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

//  The GAME step that leaves the gift's summary Gift RNG leaves open for
//  the overworld.
inline const std::string LEAVE_GIFT_SUMMARY = "leave_gift_summary";

//  The GAME step that cuts the tree the player faces, `direction`
//  (pokefirered data/scripts/field_moves.inc, EventScript_CutTree): A,
//  "This tree looks like it can be CUT down! Would you like to CUT it?"
//  YES, "<MON> used CUT!", the tree falls; then a step onto its tile. With
//  no Pokemon that knows Cut there is no YES/NO, and the step fails. The
//  tree grows back when the player leaves the map (its hide flag is a
//  FLAG_TEMP), so the walk back cuts it again.
inline const std::string CUT_TREE_VARIANT = "cut_tree";

//  How `step`, a ROUTE or LEGS, moves the player: RUN_VARIANT runs,
//  BIKE_VARIANT rides.
MoveSpeed move_speed(const StoryStep& step);

//  Converts a StoryPose to a general navigation Pose.
Pokemon::Pose to_pose(const StoryPose& pose);
//  `pose` as a StoryPose: nothing unless it is on one tile.
std::optional<StoryPose> story_pose(const Pokemon::Pose& pose);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
