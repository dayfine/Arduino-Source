/*  Scene Judges Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_SceneJudges_Tests_H
#define PokemonAutomation_Pokemon_SceneJudges_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace Pokemon{


// Registers unit tests for the judgements over screen events (warp seen, talk-over).
void add_tests_SceneJudges(UnitTestDatabase& database);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
