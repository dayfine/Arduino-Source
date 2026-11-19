/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <variant>
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

bool starts_with_game_action(const Pokemon::StoryCheckpoint& row, std::string_view action){
    if (row.steps.empty()){
        return false;
    }
    const Pokemon::StoryStep& first = row.steps.front();
    return first.kind == Pokemon::StoryStepKind::GAME && first.action == action;
}

} // namespace

bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row){
    return starts_with_game_action(row, "new_game_intro") || starts_with_game_action(row, "confirm_new_game");
}

bool story_row_makes_new_game_choices(const Pokemon::StoryCheckpoint& row){
    return starts_with_game_action(row, "new_game_intro");
}

namespace{

bool ends_on_prompt(const Pokemon::StoryStep& step){
    const auto end = step.args.find("end");
    if (step.action != "walk" || end == step.args.end()){
        return false;
    }
    const std::string* word = std::get_if<std::string>(&end->second);
    return word != nullptr && *word == "prompt";
}

//  A scene that needs nothing to start plays inside such a step.
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
                "{}: a talk must follow a step that ends on its A prompt (a walk to \"end\": \"prompt\")", step.name
            ));
        }
        if (step.action == "story_battle" && (before == nullptr || before->action != "story_battle_start")){
            errors.push_back(std::format("{}: a story battle must follow story_battle_start, which ends on the battle HUD", step.name));
        }
        if (step.kind == Pokemon::StoryStepKind::MASH && step.action == "a" && before != nullptr && plays_to_control(*before)){
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
