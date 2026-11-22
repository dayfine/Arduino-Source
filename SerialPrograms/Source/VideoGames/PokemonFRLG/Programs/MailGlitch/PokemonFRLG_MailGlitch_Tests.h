/*  Mail Glitch Activation Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Unit and OverworldSim tests for the Mail Glitch Activation program:
 *  coded indoor rooms, Retro Mail mart stock, ItemListReader names,
 *  plan loading, and end-to-end OverworldSim route verification for mg-01..mg-05.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MailGlitch_Tests_H
#define PokemonAutomation_PokemonFRLG_MailGlitch_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


void add_tests_MailGlitch(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
