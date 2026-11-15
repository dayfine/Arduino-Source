/*  MgbaLink Keys Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Keys_Tests_H
#define PokemonAutomation_Controllers_MgbaLink_Keys_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  Durations and app times to frames, the key schedule and the input log.
void add_key_tests(UnitTestDatabase& database);


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif
