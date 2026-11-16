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
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Pokemon_StoryPose.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRouteSteps.h"
#include "PokemonFRLG_AutoStoryMovedTrainers.h"
#include "PokemonFRLG_AutoStoryPoses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The trainer who saw the player on `tile` walked up to them, and stands
//  beside them from now on: adds the TRAINER_AT_FLAG that says so to
//  `run_flags`, so the routes after the battle plan around them and over
//  the tile they left (trainer_walking_up() on run_story_maps(), the
//  trainers beaten_trainers() names seeing no one). Returns the move, or
//  nothing, leaving `run_flags` as they were, if where they stand isn't known.
std::optional<TrainerMove> note_trainer_walked_up(const MapPosition& tile, std::set<std::string>& run_flags);

//  note_trainer_walked_up() after `step`, a step into a trainer's line
//  (TRAINER_LINE_VARIANT, TRAINER_WAIT_VARIANT) that ended on `outcome`'s
//  tile, logged. Any other step, or one that failed: nothing. Planned on
//  the map's tiles, 17.03's below_giovanni went north from (10, 5) through
//  Takashi, who had walked up to (10, 4) (console B, 2026-11-15 16:52).
void note_line_trainer(Logger& logger, const StoryStep& step, const Pokemon::Outcome& outcome, std::set<std::string>& run_flags);

//  Runs a step (`run`) on `state`; if it left the map it started on, the
//  trainers who walked up stand on their own tiles again, beaten
//  (trainers_back_after_map_change() on `state.flags`).
Pokemon::Outcome trainers_back_on_warp(const std::function<Pokemon::Outcome()>& run, Pokemon::RunState& state);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
