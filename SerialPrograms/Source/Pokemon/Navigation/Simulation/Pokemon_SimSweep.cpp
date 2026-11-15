/*  Sim Sweep
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_SimSweep.h"

namespace PokemonAutomation{
namespace Pokemon{


UnitTestResult sweep_result(
    Logger& logger, std::string_view row, std::string_view fault,
    size_t variants, const std::vector<SweepFailure>& failures
){
    logger.log(std::format("Fault sweep {} {}: {} variants, {} fail.", row, fault, variants, failures.size()));
    if (failures.empty()){
        return true;
    }
    std::string out = std::format("{} of {} variants fail {}:", failures.size(), variants, row);
    for (const SweepFailure& failure : failures){
        logger.log(std::format("| {} | {} | {} | {} | {} |", row, failure.step, fault, failure.variant, failure.stopped));
        out += std::format("\n  {} {}: {}", failure.step, failure.variant, failure.stopped);
    }
    return out;
}

UnitTestResult run_sweep(Logger& logger, std::string_view row, std::string_view fault, const std::vector<SweepVariant>& variants){
    std::vector<SweepFailure> failures = {};
    for (const SweepVariant& variant : variants){
        std::optional<std::string> stopped = variant.run();
        if (stopped){
            failures.push_back({variant.step, variant.label, std::move(*stopped)});
        }
    }
    return sweep_result(logger, row, fault, variants.size(), failures);
}

UnitTestResult sweep_clean_first(
    Logger& logger, std::string_view row, const std::optional<std::string>& clean_failure,
    const std::function<UnitTestResult()>& sweep
){
    if (clean_failure){
        logger.log(std::format("Fault sweep {}: not swept, the row fails clean: {}", row, *clean_failure));
        return true;
    }
    return sweep();
}

std::optional<std::string> variant_failure(const std::optional<std::string>& failure, std::string_view end, std::string_view clean_end){
    if (failure){
        return failure;
    }
    if (end != clean_end){
        return std::format("ended on {}, not {}.", end, clean_end);
    }
    return std::nullopt;
}



} // namespace Pokemon



} // namespace PokemonAutomation
