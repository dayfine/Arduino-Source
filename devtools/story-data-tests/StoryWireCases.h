/*  Story Wire Cases
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks the native story readers against PokemonSimulation's shared wire
 *  cases (format pokesim-story-wire-cases/2, data/story/wire-conformance.json),
 *  the same cases its Python reader runs.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryWireCases_H
#define PokemonAutomation_Pokemon_StoryWireCases_H

#include <cstddef>
#include <string>

namespace PokemonAutomation{
namespace Pokemon{


//  Read each case in the file at `path` with the native reader for its
//  record, and compare: a case expected rejected must fail to read; any other
//  must read the expected record. Prints one line per case and a total, and
//  returns the number of failed cases. A malformed case file throws.
size_t check_story_wire_cases(const std::string& path);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
