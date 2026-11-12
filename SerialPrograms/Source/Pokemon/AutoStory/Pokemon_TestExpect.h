/*  Test Expect
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Checks the unit tests of the AutoStory judges share.
 *
 */

#ifndef PokemonAutomation_Pokemon_TestExpect_H
#define PokemonAutomation_Pokemon_TestExpect_H

#include <format>
#include <string>
#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Passes when `got` is `expected`; else fails naming `what`.
inline UnitTestResult expect_equal(const std::string& what, const std::string& got, const std::string& expected){
    if (got == expected){
        return true;
    }
    return std::format("{}: expected \"{}\", got \"{}\"", what, expected, got);
}


}
}
#endif
