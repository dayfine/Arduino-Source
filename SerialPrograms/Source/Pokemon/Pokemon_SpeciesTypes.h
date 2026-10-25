/*  Pokemon Species Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_SpeciesTypes_H
#define PokemonAutomation_Pokemon_SpeciesTypes_H

#include <set>
#include <string>
#include <vector>

namespace PokemonAutomation{
namespace Pokemon{


//  The types of each form of a species that can be met in the wild, by its
//  name-reader slug (e.g. "muk": {poison}, {poison, dark}). Type names are lower
//  case ("flying"). Nothing for an unknown slug.
const std::vector<std::set<std::string>>* species_form_types(const std::string& slug);


}
}
#endif
