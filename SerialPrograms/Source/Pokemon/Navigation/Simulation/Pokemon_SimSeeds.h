/*  Sim Seeds
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The games' Monte Carlo rows, shared by BDSP's and FRLG's: a seeded
 *  row (wild battles at the game's odds, wandering people) runs once per
 *  seed, each seed feeding the sim's std::mt19937. Its failures are a
 *  fault sweep's (sweep_result()), a variant per seed, and it logs the
 *  mean and worst of what each seed measured.
 *
 */

#ifndef PokemonAutomation_Pokemon_SimSeeds_H
#define PokemonAutomation_Pokemon_SimSeeds_H

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Pokemon_SimSweep.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How many seeds a seeded row runs unless it asks for others.
inline constexpr uint32_t DEFAULT_SWEEP_SEEDS = 30;

//  One seed's run of a row: why it failed, or nothing; and what it measured
//  by name ("s", "stops", ...), only what applies to the seed.
struct SeedRun{
    std::optional<std::string> failure = std::nullopt;
    std::map<std::string, double> measures = {};
};

//  A measure over the seeds that passed and measured it.
struct SeedStats{
    double mean = 0;
    double worst = 0;
};

//  The seeds a row ran, those that failed, and what the rest measured.
struct SeedsResult{
    uint32_t seeds = 0;
    //  A failure per failed seed, its variant "seed N".
    std::vector<SweepFailure> failures = {};
    //  By measure, over the passed seeds; a measure no passed seed has is
    //  left out.
    std::map<std::string, SeedStats> stats = {};
};

//  Runs `run_one` from seeds 1 to `seeds`; logs "Seeds <label>: passed P/N"
//  and each measure's mean and worst (its largest). A sweep hands the
//  failures to sweep_result() with `seeds` variants.
SeedsResult run_seeds(
    Logger& logger, std::string_view label, const std::function<SeedRun(uint32_t seed)>& run_one,
    uint32_t seeds = DEFAULT_SWEEP_SEEDS
);



} // namespace Pokemon



} // namespace PokemonAutomation
#endif
