/*  AutoStory League Parity Tests
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Registration of the League parity test: every decision PokemonSimulation's
 *  planned policy made in a set of League chains
 *  (StoryData/frlg-league-parity.json, written by the sim's
 *  pokesim.cli.frlg_league_parity), made again by the program's policy.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueParity_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueParity_Tests_H

#include "Common/Cpp/TestRunners/UnitTestDatabase.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Registers the League parity test.
void add_tests_AutoStoryLeagueParity(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
