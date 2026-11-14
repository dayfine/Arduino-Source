/*  Offline FRLG Story Replay
 *  From: https://github.com/PokemonAutomation/
 */
#ifndef PokemonAutomation_PokemonFRLG_StoryReplay_H
#define PokemonAutomation_PokemonFRLG_StoryReplay_H
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{
//  Execute a captured row with the shared engine/run_route and scripted IO.
//  The supplied route is a Python candidate, never the scenario's expectation.
//  No console/profile/controller is constructed. Invalid bindings throw ParseException.
JsonValue replay_story_checkpoint(std::string_view capture, std::string_view binding,
    std::string_view candidate, const JsonValue& scenario);
//  Test-only preview resources, embedded from ReplayData/*.json with source pins.
const std::vector<std::pair<std::string, std::string>>& story_replay_fixture_json();
//  Find one embedded preview resource, or fail for an unknown fixture name.
const std::string& story_replay_fixture(std::string_view name);
//  Register adjacent native replay/conformance and negative-control tests.
void add_tests_StoryReplay(UnitTestDatabase& database);
} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
