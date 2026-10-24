#include "Programs/ShinyHunting/PokemonSV_IsolatedHuntEncounter.h"
#include "PokemonSV_IsolatedHuntEncounter_Tests.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

UnitTestResult test_isolated_hunt_encounter_options(){
    struct Case{
        IsolatedShinyAction policy;
        EncounterActionsAction expected;
    };
    const std::vector<Case> cases{
        {.policy = IsolatedShinyAction::STOP, .expected = EncounterActionsAction::STOP_PROGRAM},
        {.policy = IsolatedShinyAction::CATCH, .expected = EncounterActionsAction::THROW_BALLS},
        {.policy = IsolatedShinyAction::CATCH_AND_SAVE, .expected = EncounterActionsAction::THROW_BALLS_AND_SAVE},
    };
    EncounterBotCommonOptions base;
    base.VIDEO_ON_SHINY = false;
    for (const auto& test : cases){
        const IsolatedHuntRequest request{
            .pokemon = "charcadet", .type = IsolatedEncounterType::FIRE,
            .location = IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL,
            .ball = "luxury-ball", .ball_limit = 15, .shiny_action = test.policy,
        };
        const auto options = make_isolated_encounter_options(base, request);
        const auto entries = options->ACTIONS_TABLE.snapshot();
        if (entries.size() != 1){
            return "Expected one per-target shared encounter rule; read " + std::to_string(entries.size());
        }
        const auto& entry = entries.front();
        if (entry.pokemon != request.pokemon || entry.shininess != EncounterActionsShininess::SHINY ||
            entry.action != test.expected || entry.ball != request.ball || entry.ball_limit != request.ball_limit ||
            options->VIDEO_ON_SHINY || !base.ACTIONS_TABLE.snapshot().empty()){
            return "Expected isolated shiny action/luxury-ball/15 with base settings preserved; read " + entry.ball;
        }
    }
    return true;
}

}
}
}
