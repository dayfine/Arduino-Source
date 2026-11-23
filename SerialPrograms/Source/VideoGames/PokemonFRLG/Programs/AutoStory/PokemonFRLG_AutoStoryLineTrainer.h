/*  AutoStory Line Trainer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The trainer who saw the player on a step into their line: they walk up,
 *  and stand beside the player from then on.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLineTrainer_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLineTrainer_H

#include <functional>
#include <optional>
#include <set>
#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRouteSteps.h"
#include "PokemonFRLG_AutoStoryMovedTrainers.h"
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<TrainerMove> note_trainer_walked_up(const MapPosition& tile, std::set<std::string>& run_flags);

//  note_trainer_walked_up() after `step`, a step into a trainer's line
//  (TRAINER_LINE_VARIANT, TRAINER_WAIT_VARIANT) that succeeded and left
//  `state.pose` on one tile.
void note_line_trainer(
    Logger& logger, const StoryStep& step,
    const std::optional<Pokemon::StoryFailure>& failure, FrlgRunState& state
);

//  Runs a step (`run`) on `state`; if it left the map it started on, the
//  trainers and boulders reset for the map change.
std::optional<Pokemon::StoryFailure> trainers_back_on_warp(
    const std::function<std::optional<Pokemon::StoryFailure>()>& run, FrlgRunState& state
);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
