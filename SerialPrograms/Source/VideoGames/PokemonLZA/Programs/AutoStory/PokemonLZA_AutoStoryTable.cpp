/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <variant>
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row){
    if (row.steps.empty()){
        return false;
    }
    const Pokemon::StoryStep& first = row.steps.front();
    return first.kind == Pokemon::StoryStepKind::GAME && (first.action == "new_game_intro" || first.action == "confirm_new_game");
}

namespace{

//  Whether `step` ends on an A prompt: a walk to "prompt", or
//  walk_to_prompt.
bool ends_on_prompt(const Pokemon::StoryStep& step){
    if (step.action == "walk_to_prompt"){
        return true;
    }
    const auto end = step.args.find("end");
    if (step.action != "walk" || end == step.args.end()){
        return false;
    }
    const std::string* word = std::get_if<std::string>(&end->second);
    return word != nullptr && *word == "prompt";
}

} // namespace

std::vector<std::string_view> story_row_talks_without_prompt(const Pokemon::StoryCheckpoint& row){
    std::vector<std::string_view> talks;
    for (size_t index = 0; index < row.steps.size(); index++){
        const Pokemon::StoryStep& step = row.steps[index];
        if (step.kind == Pokemon::StoryStepKind::TALK && (index == 0 || !ends_on_prompt(row.steps[index - 1]))){
            talks.push_back(step.name);
        }
    }
    return talks;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
