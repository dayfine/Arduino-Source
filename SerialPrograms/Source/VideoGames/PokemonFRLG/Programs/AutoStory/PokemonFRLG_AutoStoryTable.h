/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed from a new game to the Boulder Badge, the Running Shoes, Route
 *  3 to the Magikarp salesman in Route 4's Pokemon Center, through Mt.
 *  Moon to Route 4 east of it, and on to Cerulean City, Misty, its rival,
 *  Nugget Bridge and Bill, then Route 6 to Vermilion City, the S.S.
 *  Anne's HM01 and Lt. Surge, as story checkpoints 00.01 to 00.25, each
 *  saved at its end. After 00.10 every route and leg runs. Taken from a by-hand run on
 *  console B (2026-11-01) and the pokefirered map scripts.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTable_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTable_H

#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Checkpoints 00.01 to 00.25, in order, with 00.01b, the way back in after
//  SID Helper, after 00.01, 00.04 split in two at Squirtle's ball for
//  Starter RNG: 00.04 and 00.04b, 00.12b, the way back in after Gift RNG,
//  after 00.12, 00.12c, the Magikarp put in BOX 1, after it, 00.14b and
//  00.14c, Cerulean Gym to a save below Misty's platform and Misty alone
//  from it, and 00.15b, the rival in Cerulean alone, from 00.15's save
//  below his trigger.
const std::vector<StoryCheckpoint>& story_checkpoints();


//  Registers AutoStory table validation unit tests.
void add_tests_AutoStory(UnitTestDatabase& database);
//  Which rows a run takes (PokemonFRLG_AutoStoryRunPlan_Tests.cpp).
void add_tests_AutoStoryRunPlan(UnitTestDatabase& database);
//  The table run end to end against a fake game (PokemonFRLG_AutoStorySimulation_Tests.cpp).
void add_tests_AutoStorySimulation(UnitTestDatabase& database);
//  Rows 00.21 and 00.22 on the overworld sim (PokemonFRLG_AutoStoryVermilion_Tests.cpp).
void add_tests_AutoStoryVermilion(UnitTestDatabase& database);
//  Rows 00.23 to 00.25 on the overworld sim (PokemonFRLG_AutoStoryVermilionGym_Tests.cpp).
void add_tests_AutoStoryVermilionGym(UnitTestDatabase& database);
//  Rows from 00.26 on on the overworld sim (PokemonFRLG_AutoStoryCeladon_Tests.cpp),
//  with add_tests_AutoStoryLavender().
void add_tests_AutoStoryCeladon(UnitTestDatabase& database);
//  Rows from 00.28 on on the overworld sim (PokemonFRLG_AutoStoryLavender_Tests.cpp).
void add_tests_AutoStoryLavender(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
