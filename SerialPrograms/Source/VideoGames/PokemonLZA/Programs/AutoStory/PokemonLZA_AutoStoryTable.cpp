/*  PokemonLZA AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
