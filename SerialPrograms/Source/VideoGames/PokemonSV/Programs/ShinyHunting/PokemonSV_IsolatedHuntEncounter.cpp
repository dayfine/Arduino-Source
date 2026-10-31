#include "Common/Cpp/Json/JsonObject.h"
#include "PokemonSV_IsolatedHuntEncounter.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
namespace{

const char* action_slug(IsolatedShinyAction action){
    switch (action){
    case IsolatedShinyAction::STOP: return "stop-program";
    case IsolatedShinyAction::CATCH: return "throw-balls";
    case IsolatedShinyAction::CATCH_AND_SAVE: return "throw-balls-and-save";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Invalid isolated hunt shiny action.");
}

}

std::unique_ptr<EncounterBotCommonOptions> make_isolated_encounter_options(
    const EncounterBotCommonOptions& base, const IsolatedHuntRequest& request
){
    auto options = std::make_unique<EncounterBotCommonOptions>();
    options->load_json(base.to_json());
    options->NOTIFICATION_NONSHINY.load_json(base.NOTIFICATION_NONSHINY.to_json());
    options->NOTIFICATION_SHINY.load_json(base.NOTIFICATION_SHINY.to_json());
    options->NOTIFICATION_CATCH_SUCCESS.load_json(base.NOTIFICATION_CATCH_SUCCESS.to_json());
    options->NOTIFICATION_CATCH_FAILED.load_json(base.NOTIFICATION_CATCH_FAILED.to_json());
    JsonObject rule;
    rule["pokemon"] = request.pokemon;
    rule["shininess"] = "shiny";
    rule["action"] = action_slug(request.shiny_action);
    rule["pokeball"] = request.ball;
    rule["ball_limit"] = request.ball_limit;
    auto row = std::make_unique<EncounterActionsRow>(options->ACTIONS_TABLE);
    row->load_json(JsonValue(std::move(rule)));
    // The shared handler applies the last matching rule; this row owns the selected target.
    options->ACTIONS_TABLE.append_row(std::move(row));
    return options;
}

}
}
}
