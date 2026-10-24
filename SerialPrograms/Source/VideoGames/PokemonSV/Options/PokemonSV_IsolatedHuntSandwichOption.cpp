#include "Common/Cpp/Json/JsonArray.h"
#include "PokemonSV_IsolatedHuntSandwichOption.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

IsolatedHuntSandwichOption::IsolatedHuntSandwichOption(OCR::LanguageOCROption* language)
    : SandwichMakerOption("<b>Sandwich:</b> Encounter Power from ordinary ingredients by default. "
        "Fire: Pickle + Olive Oil (Lv. 1). Grass: Great Klawf Claw (Lv. 2). "
        "Herba Mystica settings apply only when explicitly enabled below.",
        language, BaseRecipe::shiny, false, GroupOption::EnableMode::ALWAYS_ENABLED)
    , USE_HERBA("<b>Use Herba Mystica:</b><br>Add Sparkling Power using two selected herbs.",
        LockMode::LOCK_WHILE_RUNNING, false)
{
    PA_ADD_OPTION(USE_HERBA);
    configure_for(IsolatedEncounterType::FIRE);
}

void IsolatedHuntSandwichOption::load_json(const JsonValue& json){
    SandwichMakerOption::load_json(json);
    configure_for(TYPE == PokemonType::grass ? IsolatedEncounterType::GRASS : IsolatedEncounterType::FIRE);
}

void IsolatedHuntSandwichOption::configure_for(IsolatedEncounterType type){
    TYPE.set(type == IsolatedEncounterType::GRASS ? PokemonType::grass : PokemonType::fire);
    if (USE_HERBA){
        BASE_RECIPE.set(BaseRecipe::shiny);
    }else{
        // Standard recipes #20 and #105: no rare herbs, even with legacy shiny settings.
        // https://bulbapedia.bulbagarden.net/wiki/Sandwich_recipe
        JsonArray ingredients;
        const std::vector<std::string> recipe = type == IsolatedEncounterType::FIRE
            ? std::vector<std::string>{"pickle", "olive-oil"}
            : std::vector<std::string>{"klawf-stick", "tomato", "lettuce", "salt", "olive-oil", "wasabi"};
        for (const auto& item : recipe){ ingredients.push_back(item); }
        BASE_RECIPE.set(BaseRecipe::custom);
        SANDWICH_INGREDIENTS.load_json(JsonValue(std::move(ingredients)));
    }
    HERBA_ONE.set_visibility(ConfigOptionState::ENABLED);
    HERBA_TWO.set_visibility(ConfigOptionState::ENABLED);
    TYPE.set_visibility(ConfigOptionState::HIDDEN);
    BASE_RECIPE.set_visibility(ConfigOptionState::HIDDEN);
    SANDWICH_INGREDIENTS.set_visibility(ConfigOptionState::HIDDEN);
}

}
}
}
