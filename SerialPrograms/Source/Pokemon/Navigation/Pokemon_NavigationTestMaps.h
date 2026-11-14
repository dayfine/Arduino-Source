/*  Navigation Test Maps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_NavigationTestMaps_H
#define PokemonAutomation_Pokemon_NavigationTestMaps_H

#include <string>
#include <utility>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  (name, JSON text) of each map in Pokemon/Navigation/TestMaps/, built into
//  the program by pa_embed_text_files() (cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& navigation_test_map_json();



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
