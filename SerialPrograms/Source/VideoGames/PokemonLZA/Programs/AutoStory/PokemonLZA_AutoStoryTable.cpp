/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
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

//  Whether `step` presses through its scenes until control: a scene that
//  needs nothing from the player to start plays inside it.
bool plays_to_control(const Pokemon::StoryStep& step){
    return step.kind == Pokemon::StoryStepKind::MASH
        || step.action == "mash_to_move_hints"
        || step.action == "talk_to_pokedex_screen"
        || step.action == "talk_through_map_screen";
}

} // namespace

std::vector<std::string> story_row_lead_in_errors(const Pokemon::StoryCheckpoint& row){
    std::vector<std::string> errors;
    for (size_t index = 0; index < row.steps.size(); index++){
        const Pokemon::StoryStep& step = row.steps[index];
        const Pokemon::StoryStep* before = index == 0 ? nullptr : &row.steps[index - 1];
        if (step.kind == Pokemon::StoryStepKind::TALK && (before == nullptr || !ends_on_prompt(*before))){
            errors.push_back(std::format(
                "{}: a talk must follow a step that ends on its A prompt (a walk to \"end\": \"prompt\", or walk_to_prompt)", step.name
            ));
        }
        if (step.action == "story_battle" && (before == nullptr || before->action != "story_battle_start")){
            errors.push_back(std::format("{}: a story battle must follow story_battle_start, which ends on the battle HUD", step.name));
        }
        if (step.kind == Pokemon::StoryStepKind::MASH && before != nullptr && plays_to_control(*before)){
            errors.push_back(std::format(
                "{}: a scene must follow what starts it, not {}, which plays its scenes through to control", step.name, before->action
            ));
        }
    }
    return errors;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
