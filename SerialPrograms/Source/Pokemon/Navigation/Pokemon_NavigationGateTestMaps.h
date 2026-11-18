/*  Navigation Gate Test Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_NavigationGateTestMaps_H
#define PokemonAutomation_Pokemon_NavigationGateTestMaps_H

#include <string>
#include <utility>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  (name, JSON text) of each map in Pokemon/Navigation/GateTestMaps/, built
//  into the program by pa_embed_text_files() (cmake/EmbedTextFiles.cmake):
//  SilphCo_5F (card-key doors), PokemonMansion_1F (statue-switch barriers)
//  and PokemonMansion_3F (barriers, a statue, and holes: one-way warps),
//  trimmed from PokemonSimulation's runtime maps. 3F's hole tiles are
//  tall grass here, which the reader knows.
const std::vector<std::pair<std::string, std::string>>& navigation_gate_test_map_json();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
