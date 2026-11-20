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
#include "PokemonLZA_AutoStoryActionNames.h"
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::string_view checkpoint_first_action(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.front().action);
}

bool checkpoint_starts_without_save(const Pokemon::StoryCheckpoint& checkpoint){
    const std::string_view action = checkpoint_first_action(checkpoint);
    return action == action_name(PlzaAction::NEW_GAME_INTRO) || action == action_name(PlzaAction::CONFIRM_NEW_GAME);
}

bool checkpoint_makes_new_game_choices(const Pokemon::StoryCheckpoint& checkpoint){
    return checkpoint_first_action(checkpoint) == action_name(PlzaAction::NEW_GAME_INTRO);
}

namespace{

//  A plan step's action and its scene flags, if any.
struct LeadIn{
    std::string name = {};
    std::string action = {};
    bool stop_at_choice = false;
    bool battle_start = false;
    bool start_window = false;
    bool end_on_black_fade = false;
};

LeadIn lead_in(const JsonValue& event){
    const JsonObject& step = event.to_object_throw();
    LeadIn result{.name = step.get_string_throw("id"), .action = step.get_string_throw("action")};
    if (const JsonObject* args = step.get_object("args")){
        result.stop_at_choice = args->get_boolean_default("stop_at_choice");
        result.battle_start = args->get_boolean_default("battle_start");
        result.start_window = args->get_value("start_window_ms") != nullptr;
        result.end_on_black_fade = args->get_boolean_default("end_on_black_fade");
    }
    return result;
}

//  A scene that needs nothing to start plays inside such a step.
bool plays_to_control(const LeadIn& step){
    return (step.action == action_name(PlzaAction::SCENE) && !step.stop_at_choice && !step.battle_start && !step.end_on_black_fade) ||
        step.action == action_name(PlzaAction::TALK_TO_POKEDEX_SCREEN) || step.action == action_name(PlzaAction::TALK_THROUGH_MAP_SCREEN);
}

} // namespace

std::vector<std::string> checkpoint_lead_in_errors(const JsonObject& checkpoint){
    std::vector<std::string> errors;
    std::optional<LeadIn> before;
    for (const JsonValue& event : checkpoint.get_array_throw("events")){
        const LeadIn step = lead_in(event);
        if (step.action == action_name(PlzaAction::STORY_BATTLE) &&
            (!before || before->action != action_name(PlzaAction::SCENE) || !before->battle_start)
        ){
            errors.push_back(std::format("{}: a story battle must follow a battle_start scene, which ends on the battle HUD", step.name));
        }
        if (step.action == action_name(PlzaAction::SCENE) && step.start_window && before && plays_to_control(*before)){
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
