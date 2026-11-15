/*  Sim Seeds
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <numeric>
#include <utility>
#include "Pokemon_SimSeeds.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

//  The mean and worst (largest) of `values`, which has one at least.
SeedStats stats_of(const std::vector<double>& values){
    return {
        .mean = std::accumulate(values.begin(), values.end(), 0.) / static_cast<double>(values.size()),
        .worst = *std::max_element(values.begin(), values.end()),
    };
}

} // namespace


SeedsResult run_seeds(
    Logger& logger, std::string_view label, const std::function<SeedRun(uint32_t seed)>& run_one,
    uint32_t seeds
){
    SeedsResult out;
    out.seeds = seeds;
    std::map<std::string, std::vector<double>> passed;
    for (uint32_t seed = 1; seed <= seeds; seed++){
        SeedRun run = run_one(seed);
        if (run.failure){
            out.failures.push_back({"", std::format("seed {}", seed), std::move(*run.failure)});
            continue;
        }
        for (const auto& [measure, value] : run.measures){
            passed[measure].push_back(value);
        }
    }
    std::string line = std::format("Seeds {}: passed {}/{}", label, seeds - out.failures.size(), seeds);
    for (const auto& [measure, values] : passed){
        const SeedStats stats = stats_of(values);
        line += std::format("; {} mean {:.1f}, worst {:.1f}", measure, stats.mean, stats.worst);
        out.stats.emplace(measure, stats);
    }
    logger.log(line);
    return out;
}



} // namespace Pokemon



} // namespace PokemonAutomation
