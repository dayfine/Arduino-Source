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
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
