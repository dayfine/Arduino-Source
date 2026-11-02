#ifndef PokemonAutomation_PokemonSV_IsolatedHuntPower_H
#define PokemonAutomation_PokemonSV_IsolatedHuntPower_H
#include <string>
#include "CommonFramework/Language.h"
#include "PokemonSV_IsolatedHuntPlan.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{
class IsolatedPowerActions{
public:
    virtual ~IsolatedPowerActions() = default;
    virtual bool open_panel() = 0;
    virtual std::string read_encounter_power() = 0;
    virtual bool close_panel() = 0;
};
bool verify_isolated_power(IsolatedPowerActions& actions, Language language, IsolatedEncounterType type, uint8_t level);
}
}
}
#endif
