#include "PokemonSV_IsolatedHuntPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{

std::vector<IsolatedHuntLocationProfile> isolated_locations_for_type(IsolatedEncounterType type){
    const std::vector<IsolatedHuntLocationProfile> locations{
        {
            .location = IsolatedHuntLocation::CHARGESTONE_CAVE,
            .type = IsolatedEncounterType::GRASS,
            .pokemon = "chespin",
            .label = "Chargestone Cavern - Cave habitat",
            .start_anchor = "Central Plaza",
            .prerequisites = "Canyon diversity upgrade; Chargestone fly unlock; verify Cave spawn patch",
        },
        {
            .location = IsolatedHuntLocation::WEST_PROVINCE_ONE_CENTRAL,
            .type = IsolatedEncounterType::FIRE,
            .pokemon = "charcadet",
            .label = "West Province Area One Central - Rocky habitat",
            .start_anchor = "West Province Area One Central fly spot",
            .prerequisites = "Fly unlock; verify demonstrated Rocky spawn patch",
        },
    };
    std::vector<IsolatedHuntLocationProfile> matches;
    for (const auto& location : locations){
        if (location.type == type){
            matches.push_back(location);
        }
    }
    return matches;
}

IsolatedHuntPlan make_isolated_hunt_plan(const std::vector<IsolatedHuntRequest>& requests){
    IsolatedHuntPlan plan;
    for (const auto& request : requests){
        if (!request.enabled){
            continue;
        }
        if (request.ball.empty() || request.ball_limit == 0){
            return {.hunts = {}, .error = "Select a ball and a positive ball limit for " + request.pokemon};
        }
        bool found = false;
        for (const auto& profile : isolated_locations_for_type(request.type)){
            if (profile.location == request.location && profile.pokemon == request.pokemon){
                found = true;
            }
        }
        if (!found){
            return {.hunts = {}, .error = "Unsupported Pokemon/type/location combination: " + request.pokemon};
        }
        plan.hunts.push_back(request);
    }
    if (plan.hunts.empty()){
        return {.hunts = {}, .error = "Enable at least one hunt."};
    }
    return plan;
}

}
}
}
