/*  MgbaLink Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_Tests_H
#define PokemonAutomation_Controllers_MgbaLink_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  Reply parsing, line framing, the frame clock, and a connection against a
//  fake bridge on 127.0.0.1.
void add_tests(UnitTestDatabase& database);


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif
