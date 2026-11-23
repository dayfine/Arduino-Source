/*  AutoStory Save Checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether a loaded save starts a checkpoint: the save card's place and the
 *  pose the save stands at.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStorySaveChecks_H
#define PokemonAutomation_PokemonFRLG_AutoStorySaveChecks_H

#include <functional>
#include <optional>
#include <string>
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_SaveCardReader.h"
#include "PokemonFRLG_AutoStoryPoses.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::string save_place_error(const std::string& start_map, const std::string& place);
std::string checkpoint_end_map(const std::string& id);
std::string save_end_error(const std::string& end_map, const std::string& place);
std::string start_pose_mismatch(const Pokemon::GridPose& start, const StoryPose& stands);

using SavePoseReader = std::function<std::optional<StoryPose>()>;
using StoryLog = std::function<void(const std::string& line)>;

std::optional<Pokemon::StoryFailure> run_start_pose_check(
    const std::optional<Pokemon::GridPose>& start, const SavePoseReader& read, const StoryLog& log
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
