/*  AutoStory Poses
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Where the story stands and how fast it moves: its poses, action names,
 *  and movement variants.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPoses_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPoses_H

#include <optional>
#include <string>
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryStepTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The variant of a route or legs step that runs, with B held: the player
//  has the Running Shoes (03.03 on).
inline const std::string RUN_VARIANT = "run";

//  The action that closes the Trainer Card SID Helper leaves open.
inline const std::string CLOSE_TRAINER_CARD = "close_trainer_card";

//  The action that leaves the gift's summary Gift RNG leaves open for
//  the overworld.
inline const std::string LEAVE_GIFT_SUMMARY = "leave_gift_summary";

//  The action that cuts the tree the player faces, `direction`.
inline const std::string CUT_TREE_VARIANT = "cut_tree";

//  The action that pushes the boulder the player faces, `direction`,
//  `tiles` tiles on.
inline const std::string PUSH_BOULDER_VARIANT = "push_boulder";

//  How `step`, a route or legs step, moves the player: RUN_VARIANT runs,
//  BIKE_VARIANT rides.
MoveSpeed move_speed(const StoryStep& step);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
