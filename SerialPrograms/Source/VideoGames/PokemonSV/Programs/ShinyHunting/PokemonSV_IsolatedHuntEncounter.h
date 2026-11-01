#ifndef PokemonAutomation_PokemonSV_IsolatedHuntEncounter_H
#define PokemonAutomation_PokemonSV_IsolatedHuntEncounter_H
#include "VideoGames/PokemonSV/Options/PokemonSV_EncounterBotCommon.h"
#include "PokemonSV_IsolatedHuntPlan.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
std::unique_ptr<EncounterBotCommonOptions> make_isolated_encounter_options(
    const EncounterBotCommonOptions& base, const IsolatedHuntRequest& request
);
}
}
}
#endif
