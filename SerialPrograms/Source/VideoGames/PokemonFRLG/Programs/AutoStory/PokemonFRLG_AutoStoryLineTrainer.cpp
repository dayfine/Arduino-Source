/*  AutoStory Line Trainer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonFRLG_AutoStoryMovedBoulders.h"
#include "PokemonFRLG_AutoStoryLineTrainer.h"
#include "PokemonFRLG_AutoStoryPacedRoute.h"
#include "PokemonFRLG_AutoStoryTrainerLine.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::optional<TrainerMove> note_trainer_walked_up(const MapPosition& tile, std::set<std::string>& run_flags){
    const std::optional<TrainerMove> move = trainer_walking_up(run_story_maps(run_flags), tile, beaten_trainers(run_flags));
    if (move){
        run_flags.insert(trainer_at_flag(*move));
    }
    return move;
}

void note_line_trainer(
    Logger& logger, const StoryStep& step,
    const std::optional<Pokemon::StoryFailure>& failure, FrlgRunState& state
){
    const bool line = step.action == "step"
        && (step.variant == TRAINER_LINE_VARIANT || step.variant == TRAINER_WAIT_VARIANT);
    if (!line || failure){
        return;
    }
    const std::optional<StoryPose> at = story_pose(state.pose);
    if (!at){
        return;
    }
    const MapPosition tile{at->map, at->tile};
    if (const std::optional<TrainerMove> move = note_trainer_walked_up(tile, state.flags)){
        logger.log(std::format("Story: {}: {} walked up to ({}, {}) and stays there.", step.name, move->trainer, move->x, move->y));
    }else{
        logger.log(std::format("Story: {}: where the trainer who saw the player on {} stands now isn't known.", step.name, to_string(tile)), COLOR_ORANGE);
    }
}

std::optional<Pokemon::StoryFailure> trainers_back_on_warp(
    const std::function<std::optional<Pokemon::StoryFailure>()>& run, FrlgRunState& state
){
    const std::string map_before = state.pose ? state.pose->map : std::string();
    std::optional<Pokemon::StoryFailure> failure = run();
    const std::string map_after = state.pose ? state.pose->map : std::string();
    trainers_back_after_map_change(map_before, map_after, state.flags);
    boulders_back_after_map_change(map_before, map_after, state.flags);
    return failure;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
