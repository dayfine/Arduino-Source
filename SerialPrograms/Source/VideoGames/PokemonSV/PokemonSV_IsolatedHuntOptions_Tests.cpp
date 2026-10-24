#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Options/PokemonSV_IsolatedHuntTable.h"
#include "Options/PokemonSV_IsolatedHuntSandwichOption.h"
#include "PokemonSV_IsolatedHuntOptions_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

UnitTestResult test_isolated_hunt_options(){
    IsolatedHuntTable table;
    auto defaults = table.snapshot();
    if (defaults.size() != 1 || defaults[0].pokemon != "charcadet" || defaults[0].shiny_action != IsolatedShinyAction::STOP){
        return "Expected one default Charcadet row with stop-on-shiny policy.";
    }
    JsonObject row;
    row["enabled"] = true;
    row["pokemon"] = "chespin";
    row["type"] = "grass";
    row["location"] = "chargestone-cave";
    row["shiny_action"] = "catch-and-save";
    row["ball"] = "friend-ball";
    row["ball_limit"] = 20;
    JsonArray rows;
    rows.push_back(std::move(row));
    table.load_json(JsonValue(std::move(rows)));
    const auto request = table.snapshot().front();
    if (request.pokemon != "chespin" || request.type != IsolatedEncounterType::GRASS ||
        request.location != IsolatedHuntLocation::CHARGESTONE_CAVE ||
        request.shiny_action != IsolatedShinyAction::CATCH_AND_SAVE ||
        request.ball != "friend-ball" || request.ball_limit != 20){
        return "Expected saved Chespin/Grass/Cave/catch-save/friend-ball/20 options; read " + table.to_json().dump();
    }
    IsolatedHuntTable restored;
    restored.load_json(table.to_json());
    const auto plan = make_isolated_hunt_plan(restored.snapshot());
    if (!plan.error.empty() || plan.hunts.size() != 1){
        return "Expected valid round-tripped hunt options; read " + plan.error;
    }
    return true;
}

UnitTestResult test_isolated_hunt_sandwich(){
    IsolatedHuntSandwichOption option(nullptr);
    if (option.USE_HERBA || option.BASE_RECIPE != BaseRecipe::custom){
        return "Expected ordinary ingredients and Herba Mystica disabled by default; read " + option.to_json().dump();
    }
    struct Case{
        IsolatedEncounterType type;
        std::string ingredients;
    };
    const Case cases[]{
        {.type = IsolatedEncounterType::FIRE, .ingredients = "[\"pickle\",\"olive-oil\"]"},
        {.type = IsolatedEncounterType::GRASS, .ingredients = "[\"klawf-stick\",\"tomato\",\"lettuce\",\"salt\",\"olive-oil\",\"wasabi\"]"},
    };
    for (const auto& test : cases){
        option.configure_for(test.type);
        std::vector<std::string> actual;
        for (const auto& row : option.SANDWICH_INGREDIENTS.copy_snapshot()){
            actual.push_back(row->item.slug());
        }
        JsonArray actual_json;
        for (const auto& item : actual){ actual_json.push_back(item); }
        if (actual_json.dump(-1) != test.ingredients){
            return "Expected ordinary recipe " + test.ingredients + "; read " + actual_json.dump();
        }
    }
    JsonObject legacy;
    legacy["BASE_RECIPE"] = "shiny";
    IsolatedHuntSandwichOption restored(nullptr);
    restored.load_json(JsonValue(std::move(legacy)));
    if (restored.USE_HERBA || restored.BASE_RECIPE != BaseRecipe::custom){
        return "Expected legacy shiny settings to require explicit herb opt-in; read " + restored.to_json().dump();
    }
    restored.USE_HERBA = true;
    restored.configure_for(IsolatedEncounterType::GRASS);
    if (restored.BASE_RECIPE != BaseRecipe::shiny || restored.TYPE != PokemonType::grass){
        return "Expected opted-in Grass Sparkling recipe; read " + restored.to_json().dump();
    }
    restored.USE_HERBA = false;
    restored.configure_for(IsolatedEncounterType::FIRE);
    if (restored.BASE_RECIPE != BaseRecipe::custom){
        return "Expected disabling herb opt-in to restore ordinary recipe; read " + restored.to_json().dump();
    }
    return true;
}

}
}
}
