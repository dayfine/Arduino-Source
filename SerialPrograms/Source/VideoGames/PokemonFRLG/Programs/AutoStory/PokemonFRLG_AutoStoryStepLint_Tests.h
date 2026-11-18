/*  AutoStory Step Lint Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the lints over the story table
 *  (PokemonFRLG_AutoStoryStepLint_Tests.cpp).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStepLint_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStepLint_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The story table's lints: declared ends chain, no ANSWER after a walk,
//  no new legs event without a reason.
void add_tests_AutoStoryStepLint(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
