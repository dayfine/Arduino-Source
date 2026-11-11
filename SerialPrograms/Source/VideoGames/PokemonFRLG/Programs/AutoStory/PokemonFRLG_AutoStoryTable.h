/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed from a new game to the Boulder Badge, the Running Shoes, Route
 *  3 to the Magikarp salesman in Route 4's Pokemon Center, through Mt.
 *  Moon to Route 4 east of it, and on to Cerulean City, Misty, its rival,
 *  Nugget Bridge and Bill, then Route 6 to Vermilion City, the S.S.
 *  Anne's HM01 and Lt. Surge, as story checkpoints 01.01 to 06.04, each
 *  saved at its end. After 03.03 every route and leg runs. Taken from a by-hand run on
 *  console B (2026-11-01) and the pokefirered map scripts.
 *
 *  An id is part.NN, checkpoint NN of that part of Bulbapedia's FireRed
 *  and LeafGreen walkthrough (parts 1 to 9 here). The order follows our
 *  setup and can differ in other runs: the rival battles depend on the
 *  starter, Route 11 and Diglett's Cave are skipped, the Bike Voucher is
 *  fetched as a detour, and the Rocket Hideout comes before Celadon Gym.
 *
 *  Each checkpoint is an authored JSON file in StoryData/
 *  (frlg-<id>.plan.json, format pokesim-authored-checkpoint/1): the shared
 *  checkpoint, its steps each an action with args, read against FRLG's
 *  actions (story_action_specs()), and FRLG's execution object:
 *
 *      "menu"      before_pokemon, before_pokedex or full: the START menu
 *                  at its end (StoryCheckpoint::menu);
 *      "supplies"  optional: {"potions", "antidotes", "parlyz_heals"} at
 *                  its start, for a run that loads its save;
 *      "lead"      optional: "after_brock", the starter as the story left
 *                  it by Brock (story_lead_after_brock());
 *      "handoff"   optional: sid_helper, starter_rng or gift_rng, the RNG program
 *                  the user may run there (StoryCheckpoint::handoff);
 *      "live_start" optional, true: it starts from the game as another
 *                  program left it (StoryCheckpoint::live_start);
 *      "notes"     optional: {"checkpoint" or a step's id: [lines]}, the
 *                  reasons behind a step that a reader of the file needs.
 *                  The program doesn't read them.
 *
 *  Beside "execution", "metadata": {"phase": testing, verification or
 *  reproduction}: how far the checkpoint is trusted.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryTable_H
#define PokemonAutomation_PokemonFRLG_AutoStoryTable_H

#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  (name, JSON text) of each file in StoryData/, sorted by name.
//  Generated at build time (SerialPrograms/cmake/EmbedTextFiles.cmake).
const std::vector<std::pair<std::string, std::string>>& autostory_checkpoint_json();

//  The checkpoint of an authored FRLG file. Throws ParseException when
//  the file is malformed, names an action FRLG doesn't have, gives an
//  action args it doesn't take, or has notes for a step it doesn't have.
StoryCheckpoint read_story_checkpoint_file(const JsonValue& value, std::string_view source);

//  Checkpoints 01.01 to 07.02, in order: every file in StoryData/. With
//  01.02, the way back in after SID Helper, after 01.01, 01.05 split in two at Squirtle's ball for
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



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
