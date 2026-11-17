/*  PokemonLZA AutoStory - Wild Zone 1 road
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

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

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
