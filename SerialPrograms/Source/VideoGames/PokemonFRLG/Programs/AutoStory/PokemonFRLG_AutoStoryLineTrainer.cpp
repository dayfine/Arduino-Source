/*  AutoStory Line Trainer
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <vector>
#include "Pokemon/AutoStory/Pokemon_TilePose.h"
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

void note_line_trainer(Logger& logger, const StoryStep& step, const Pokemon::Outcome& outcome, std::set<std::string>& run_flags){
    const bool line = step.action == TRAINER_LINE_VARIANT || step.action == TRAINER_WAIT_VARIANT;
    const std::vector<TilePos> tiles = line && outcome.succeeded() && outcome.pose ? Pokemon::tiles_of(*outcome.pose) : std::vector<TilePos>();
    if (tiles.size() != 1){
        return;
    }
    const MapPosition tile{outcome.pose->map, tiles[0]};
    if (const std::optional<TrainerMove> move = note_trainer_walked_up(tile, run_flags)){
        logger.log(std::format("Story: {}: {} walked up to ({}, {}) and stays there.", step.name, move->trainer, move->x, move->y));
    }else{
        logger.log(std::format("Story: {}: where the trainer who saw the player on {} stands now isn't known.", step.name, to_string(tile)), COLOR_ORANGE);
    }
}


Pokemon::Outcome trainers_back_on_warp(const std::function<Pokemon::Outcome()>& run, Pokemon::RunState& state){
    const std::string map_before = state.pose.map;
    Pokemon::Outcome outcome = run();
    trainers_back_after_map_change(map_before, state.pose.map, state.flags);
    boulders_back_after_map_change(map_before, state.pose.map, state.flags);
    return outcome;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
