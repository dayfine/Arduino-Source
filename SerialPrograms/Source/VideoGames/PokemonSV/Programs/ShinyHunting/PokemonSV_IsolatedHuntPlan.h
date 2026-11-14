#ifndef PokemonAutomation_PokemonSV_IsolatedHuntPlan_H
#define PokemonAutomation_PokemonSV_IsolatedHuntPlan_H

#include <cstdint>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

enum class IsolatedShinyAction{ STOP, CATCH, CATCH_AND_SAVE };

enum class IsolatedEncounterType{ GRASS, FIRE };
enum class IsolatedHuntLocation{ CHARGESTONE_CAVE, WEST_PROVINCE_ONE_CENTRAL };

struct IsolatedHuntRequest{
    std::string pokemon;
    IsolatedEncounterType type;
    IsolatedHuntLocation location;
    std::string ball;
    uint16_t ball_limit = 40;
    bool enabled = true;
    IsolatedShinyAction shiny_action = IsolatedShinyAction::STOP;
};

struct IsolatedHuntLocationProfile{
    IsolatedHuntLocation location;
    IsolatedEncounterType type;
    std::string pokemon;
    std::string label;
    std::string start_anchor;
    std::string prerequisites;
};

std::vector<IsolatedHuntLocationProfile> isolated_locations_for_type(IsolatedEncounterType type);

struct IsolatedHuntPlan{
    std::vector<IsolatedHuntRequest> hunts;
    std::string error;
};

IsolatedHuntPlan make_isolated_hunt_plan(const std::vector<IsolatedHuntRequest>& requests);

}
}
}
#endif
