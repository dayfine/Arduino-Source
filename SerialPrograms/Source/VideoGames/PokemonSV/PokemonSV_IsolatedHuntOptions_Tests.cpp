#include "Common/Cpp/Json/JsonArray.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Options/PokemonSV_IsolatedHuntTable.h"
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

}
}
}
