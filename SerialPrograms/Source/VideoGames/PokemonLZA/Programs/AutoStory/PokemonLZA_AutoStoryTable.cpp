/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string_view>
#include "PokemonLZA_AutoStoryGame.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using Pokemon::StoryCheckpoint;
using Pokemon::StoryStep;
using Pokemon::StoryStepKind;


bool story_row_saves(const StoryData& story, const StoryCheckpoint& row){
    return !story.autosaves.contains(row.id);
}

size_t story_rows_ready(const std::vector<StoryCheckpoint>& checkpoints){
    size_t ready = 0;
    for (const StoryCheckpoint& checkpoint : checkpoints){
        for (const StoryStep& step : checkpoint.steps){
            if (step.kind == StoryStepKind::GAME && story_actions_not_implemented().contains(step.action)){
                return ready;
            }
        }
        ready++;
    }
    return ready;
}

namespace{

//  Whether `row`'s first step is the GAME action `action`.
bool first_game_action_is(const StoryCheckpoint& row, std::string_view action){
    if (row.steps.empty()){
        return false;
    }
    const StoryStep& first = row.steps.front();
    return first.kind == StoryStepKind::GAME && first.action == action;
}

} // namespace

bool story_row_starts_without_save(const StoryCheckpoint& row){
    return first_game_action_is(row, "confirm_new_game");
}

std::vector<StoryCheckpoint> story_run_rows(const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last){
    if (first > last || last >= checkpoints.size()){
        return {};
    }
    return std::vector<StoryCheckpoint>(checkpoints.begin() + first, checkpoints.begin() + last + 1);
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
