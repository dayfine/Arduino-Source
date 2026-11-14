/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <functional>
#include "PokemonLZA_AutoStoryZone1Road.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


std::vector<int> alternating_stops(std::span<const int> spots, size_t visits){
    std::vector<int> stops;
    if (spots.empty()){
        return stops;
    }
    for (size_t i = 0; i < visits; i++){
        stops.push_back(spots[i % spots.size()]);
    }
    return stops;
}

std::vector<int> road_stops_to_gate(std::span<const int> spots, std::optional<int> distance){
    std::vector<int> stops(spots.begin(), spots.end());
    std::sort(stops.begin(), stops.end(), std::greater<int>());
    if (!distance || stops.empty()){
        return stops;
    }
    std::vector<int> ahead;
    for (int stop : stops){
        if (stop < *distance){
            ahead.push_back(stop);
        }
    }
    if (ahead.empty()){
        ahead.push_back(stops.back());
    }
    return ahead;
}

bool starts_in_zone_1(std::optional<int> distance){
    return distance && *distance <= ZONE_1_START_MAX;
}

bool left_zone_1(std::optional<int> distance){
    return distance && *distance > ZONE_1_START_MAX;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
