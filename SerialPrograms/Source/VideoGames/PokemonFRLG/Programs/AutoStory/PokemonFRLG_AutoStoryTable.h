/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed from a new game to the Boulder Badge, the Running Shoes, Route
 *  3 to the Magikarp salesman in Route 4's Pokemon Center, through Mt.
 *  Moon to Route 4 east of it, and on to Cerulean City, Misty, its rival,
 *  Nugget Bridge and Bill, then Route 6 to Vermilion City, the S.S.
 *  Anne's HM01 and Lt. Surge, on to Rock Tunnel and Lavender Town, as
 *  story checkpoints 01.01 to 08.02, each saved at its end. After 03.03
 *  every route and leg runs. Taken from a by-hand run on console B
 *  (2026-11-01) and the pokefirered map scripts.
 *
 *  Each checkpoint is an authored plan, StoryData/frlg-<id>.plan.json
 *  (02.01: frlg-parcel-02.01.plan.json), built into the program and
 *  loaded by load_authored_story() (Data/JsonParser.h). Its sections, the
 *  notes on how each step was planned and why, live there too.
 *
 *  An id is part.NN, checkpoint NN of that part of Bulbapedia's FireRed
 *  and LeafGreen walkthrough (parts 1 to 9 here). The order follows our
 *  setup and can differ in other runs: the rival battles depend on the
 *  starter, Route 11 and Diglett's Cave are skipped, the Bike Voucher is
 *  fetched as a detour, and the Rocket Hideout comes before Celadon Gym.
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


//  Checkpoints 01.01 to 08.02, in order of id, as authored in StoryData,
//  with 01.02, the way back in after SID Helper, after 01.01, 01.05 split in two at Squirtle's ball for
//  Starter RNG: 01.05 and 01.06, 04.03, the way back in after Gift RNG,
//  after 04.02, 04.04, the Magikarp put in BOX 1, after it, 05.01 and
//  05.02, Cerulean Gym to a save below Misty's platform and Misty alone
//  from it, and 05.04, the rival in Cerulean alone, from 05.03's save
//  below his trigger.
const std::vector<StoryCheckpoint>& story_checkpoints();


//  Registers AutoStory table validation unit tests.
void add_tests_AutoStory(UnitTestDatabase& database);
//  Which rows a run takes (PokemonFRLG_AutoStoryRunPlan_Tests.cpp).
void add_tests_AutoStoryRunPlan(UnitTestDatabase& database);
//  The table run end to end against a fake game (PokemonFRLG_AutoStorySimulation_Tests.cpp).
void add_tests_AutoStorySimulation(UnitTestDatabase& database);
//  Rows 05.10 and 06.01 on the overworld sim (PokemonFRLG_AutoStoryVermilion_Tests.cpp).
void add_tests_AutoStoryVermilion(UnitTestDatabase& database);
//  Rows 06.02 to 06.04 on the overworld sim (PokemonFRLG_AutoStoryVermilionGym_Tests.cpp).
void add_tests_AutoStoryVermilionGym(UnitTestDatabase& database);
//  Rows from 07.01 on on the overworld sim (PokemonFRLG_AutoStoryCeladon_Tests.cpp),
//  with add_tests_AutoStoryLavender().
void add_tests_AutoStoryCeladon(UnitTestDatabase& database);
//  Rows from 08.01 on on the overworld sim (PokemonFRLG_AutoStoryLavender_Tests.cpp).
void add_tests_AutoStoryLavender(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
