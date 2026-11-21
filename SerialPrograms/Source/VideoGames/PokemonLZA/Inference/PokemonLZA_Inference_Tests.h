/*  PokemonLZA Inference - Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_Inference_Tests_H
#define PokemonAutomation_PokemonLZA_Inference_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Full-build suites (image/screenshot detectors).
void add_tests_Detectors(UnitTestDatabase& database);
void add_tests_DialogDetectors(UnitTestDatabase& database);

inline void add_tests_InferenceImage(UnitTestDatabase& database){
    add_tests_Detectors(database);
    add_tests_DialogDetectors(database);
}


} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
