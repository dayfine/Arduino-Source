/*  AutoStory Endpoint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the tests that walk each checkpoint on the overworld
 *  sim to its declared end, and of the test that every checkpoint has one.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryEndpoints_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryEndpoints_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Each checkpoint walked on the overworld sim to its declared end, and the
//  test that every checkpoint has such a case or an exemption
//  (PokemonFRLG_AutoStoryEndpoints_Tests.cpp).
void add_tests_AutoStoryEndpoints(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
