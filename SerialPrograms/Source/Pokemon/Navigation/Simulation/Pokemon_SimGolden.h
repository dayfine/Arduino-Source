/*  Sim Golden
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Golden files for the games' overworld simulations: what a simulation
 *  observably does (the legs it walks, where steps end, the events it
 *  shows, how injected faults turn out), written as text a line per fact
 *  and checked in beside the test that writes it. A refactor of a
 *  simulation that should change nothing must leave its golden as it is;
 *  a change that should, shows as the golden's diff in review.
 *
 *  A golden test builds its text and hands it to check_sim_golden(). With
 *  SIM_GOLDEN_UPDATE_VARIABLE set to anything but "" or "0", the golden is
 *  rewritten instead, with CRLF line endings as the source tree keeps.
 *
 */

#ifndef PokemonAutomation_Pokemon_SimGolden_H
#define PokemonAutomation_Pokemon_SimGolden_H

#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Set it to rewrite the golden files instead of checking them.
inline constexpr std::string_view SIM_GOLDEN_UPDATE_VARIABLE = "PA_UPDATE_SIM_GOLDENS";

//  Whether SIM_GOLDEN_UPDATE_VARIABLE asks for the goldens to be rewritten.
bool sim_golden_update_requested();

//  The file `name` in the directory of `source_file`, a test's __FILE__.
std::string sim_golden_path(std::string_view source_file, std::string_view name);

//  The golden text: `about` as a "# " comment line, then `lines`, each
//  ended by "\n".
std::string sim_golden_text(std::string_view about, const std::vector<std::string>& lines);

//  The first line where `golden` and `actual` differ, carriage returns
//  ignored: "line N:\n--- golden: ...\n+++ actual: ...". "" if none.
std::string first_golden_difference(std::string_view golden, std::string_view actual);

//  Checks `text` against the golden file at `path`, or rewrites the file
//  with it when SIM_GOLDEN_UPDATE_VARIABLE is set. Fails with the first
//  difference, or with how to write a golden that is missing.
UnitTestResult check_sim_golden(Logger& logger, const std::string& path, const std::string& text);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
