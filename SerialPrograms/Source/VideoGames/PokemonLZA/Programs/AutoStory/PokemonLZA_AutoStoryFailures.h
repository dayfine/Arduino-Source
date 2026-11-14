/*  PokemonLZA AutoStory - Failures
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  PLZA's own step failure names, each with its class in the shared list
 *  (Pokemon_StoryFailureClasses.h), so the run log tags a PLZA failure
 *  "(class=<id>)" as it does the engine's.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryFailures_H
#define PokemonAutomation_PokemonLZA_AutoStoryFailures_H

#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailureClasses.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  One of PLZA's failure names and its class.
struct PlzaFailure{
    std::string_view name;
    Pokemon::StoryFailureClass failure_class;
};

//  Every failure name PLZA's steps report that the shared list has no class
//  for, with the class it takes.
const std::vector<PlzaFailure>& plza_failures();

//  The class of PLZA's failure `name`; nothing for a name not in
//  plza_failures() (the shared list's names, "operation_failed", keep theirs).
std::optional<Pokemon::StoryFailureClass> plza_failure_class(std::string_view name);

//  Pokemon::outcome_failed() with plza_failure_class()'s class for `name`.
Pokemon::Outcome plza_failed(std::string name, std::string detail);

//  A console step's failure name when it throws: its own where a console
//  run showed what its failures mean, else "operation_failed".
std::string_view console_step_failure(std::string_view action);

//  Registers the tests of the names' classes.
void add_tests_AutoStoryFailures(UnitTestDatabase& database);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
