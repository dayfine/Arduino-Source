/*  MgbaLink Key Sender Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Controllers_MgbaLink_KeySender_Tests_H
#define PokemonAutomation_Controllers_MgbaLink_KeySender_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace MgbaLink{


//  The shared link registry, and the pacer and key sender against a fake
//  bridge on 127.0.0.1.
void add_key_sender_tests(UnitTestDatabase& database);


}  // namespace MgbaLink
}  // namespace PokemonAutomation
#endif
