/*  AutoStory Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed from a new game to the Boulder Badge and the Running Shoes, as
 *  story checkpoints 00.01 to 00.10, each saved at its end. After 00.10
 *  every route and leg runs. Taken from a by-hand run on
 *  console B (2026-11-01) and the pokefirered map scripts.
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

//  Checkpoints 00.01 to 00.10, in order: every file in StoryData/.
const std::vector<StoryCheckpoint>& story_checkpoints();


//  Registers AutoStory table validation unit tests.
void add_tests_AutoStory(UnitTestDatabase& database);
//  The table run end to end against a fake game (PokemonFRLG_AutoStorySimulation_Tests.cpp).
void add_tests_AutoStorySimulation(UnitTestDatabase& database);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
