/*  FRLG Authored Story Tests
 *  From: https://github.com/PokemonAutomation/
 */

#ifndef PokemonAutomation_PokemonFRLG_AuthoredStory_Tests_H
#define PokemonAutomation_PokemonFRLG_AuthoredStory_Tests_H

#include <string>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "VideoGames/PokemonFRLG/Programs/AutoStory/PokemonFRLG_AutoStoryEvents.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The whole story of `table` on the story maps (StoryMapsSim), every
//  handoff off: each step's end, each event's log line without its
//  section, each save's pose, and the steps the sim stubbed.
std::vector<std::string> story_sim_trace(const std::vector<StoryCheckpoint>& table);

//  The authored story table against its golden trace.
void add_tests_AuthoredStory(UnitTestDatabase& database);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
