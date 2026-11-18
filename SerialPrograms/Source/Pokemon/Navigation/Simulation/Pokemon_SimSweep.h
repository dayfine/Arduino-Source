/*  Sim Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The runner of the games' fault sweeps, shared by BDSP's and FRLG's: a
 *  sweep injects one class of fault into one story row, a variant at a
 *  time (each place or way the fault can happen), and fails with the
 *  variants the row does not survive. Which variants a class has and how
 *  each runs on a game's sim stay the game's; the runner owns the rule
 *  that a row failing clean is not swept, the comparison of a variant's
 *  run with the clean one, and the result's log lines and message.
 *
 */

#ifndef PokemonAutomation_Pokemon_SimSweep_H
#define PokemonAutomation_Pokemon_SimSweep_H

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTest.h"

namespace PokemonAutomation{
namespace Pokemon{


//  One injected variant that failed its row: the step it was injected at
//  ("" for one over the whole row), what was injected, and how the row
//  stopped or where it ended instead.
struct SweepFailure{
    std::string step;
    std::string variant;
    std::string stopped;
};

//  The sweep of class `fault` (its name for logs) over row `row`: passes
//  with no failures. Logs "Fault sweep <row> <fault>: N variants, M fail."
//  and a table line per failure, "| row | step | class | variant | stopped |".
UnitTestResult sweep_result(
    Logger& logger, std::string_view row, std::string_view fault,
    size_t variants, const std::vector<SweepFailure>& failures
);


//  One variant to sweep: its step, its label, and the run of the row with
//  it injected, giving why the row failed with it (variant_failure()) or
//  nothing.
struct SweepVariant{
    std::string step;
    std::string label;
    std::function<std::optional<std::string>()> run;
};

//  Runs every variant of class `fault` over row `row`; sweep_result() of
//  those that failed.
UnitTestResult run_sweep(Logger& logger, std::string_view row, std::string_view fault, const std::vector<SweepVariant>& variants);

//  `sweep` of row `row`, unless the row fails clean (`clean_failure`, why):
//  a row that fails clean is not swept and passes, its own test reports it.
UnitTestResult sweep_clean_first(
    Logger& logger, std::string_view row, const std::optional<std::string>& clean_failure,
    const std::function<UnitTestResult()>& sweep
);

//  Why a variant's run failed its row: its own failure if it has one, else
//  ending on `end`, not the clean run's `clean_end` (both as the game
//  prints a position).
std::optional<std::string> variant_failure(const std::optional<std::string>& failure, std::string_view end, std::string_view clean_end);



} // namespace Pokemon



} // namespace PokemonAutomation
#endif
