/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <string>
#include <string_view>
#include <optional>
#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::string_view story_row_first_action(const Pokemon::StoryCheckpoint& row){
    return row.steps.empty() ? std::string_view() : std::string_view(row.steps.front().action);
}

bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row){
    const std::string_view action = story_row_first_action(row);
    return action == "new_game_intro" || action == "confirm_new_game";
}

bool story_row_makes_new_game_choices(const Pokemon::StoryCheckpoint& row){
    return story_row_first_action(row) == "new_game_intro";
}

namespace{

//  A plan step's action and its walk's "end", if any.
struct LeadIn{
    std::string name = {};
    std::string action = {};
    std::string end = {};
};

LeadIn lead_in(const JsonValue& event){
    const JsonObject& step = event.to_object_throw();
    LeadIn result{.name = step.get_string_throw("id"), .action = step.get_string_throw("action")};
    const JsonObject* args = step.get_object("args");
    const JsonValue* end = args == nullptr ? nullptr : args->get_value("end");
    if (end != nullptr && end->to_string() != nullptr){
        result.end = *end->to_string();
    }
    return result;
}

//  A scene that needs nothing to start plays inside such a step.
bool plays_to_control(const LeadIn& step){
    return step.action == "mash" || step.action == "a"
        || step.action == "mash_to_move_hints"
        || step.action == "talk_to_pokedex_screen"
        || step.action == "talk_through_map_screen";
}

} // namespace

std::vector<std::string> story_row_lead_in_errors(const JsonObject& checkpoint){
    std::vector<std::string> errors;
    std::optional<LeadIn> before;
    for (const JsonValue& event : checkpoint.get_array_throw("events")){
        const LeadIn step = lead_in(event);
        if (step.action == "talk" && (!before || before->action != "walk" || before->end != "prompt")){
            errors.push_back(std::format(
                "{}: a talk must follow a step that ends on its A prompt (a walk to \"end\": \"prompt\")", step.name
            ));
        }
        if (step.action == "story_battle" && (!before || before->action != "story_battle_start")){
            errors.push_back(std::format("{}: a story battle must follow story_battle_start, which ends on the battle HUD", step.name));
        }
        if (step.action == "a" && before && plays_to_control(*before)){
            errors.push_back(std::format(
                "{}: a scene must follow what starts it, not {}, which plays its scenes through to control", step.name, before->action
            ));
        }
        before = step;
    }
    return errors;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
