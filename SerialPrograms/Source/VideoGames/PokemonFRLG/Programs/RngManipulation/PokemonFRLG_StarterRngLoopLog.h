/*  Starter RNG Loop Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How the Starter RNG loop reports: an attempt's outcome as the plan counts
 *  it, the loop's state line and each reset's line.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngLoopLog_H
#define PokemonAutomation_PokemonFRLG_StarterRngLoopLog_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "PokemonFRLG_RngPlan.h"
#include "PokemonFRLG_StarterRngOutcome.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Starter RNG's outcome as the RNG plan counts it. Only a shiny on the
//  target is SHINY; one off it, or not known to be on it, is
//  SHINY_OFF_TARGET. Out of resets, no matches and a wild shiny are misses,
//  which the plan runs again.
RngAttemptOutcome plan_outcome(StarterRngOutcome outcome);
//  As the result line names it: "shiny_on_target", "target_hit",
//  "out_of_resets"... (The loop says "attempt_deadline" instead of
//  "out_of_resets" once the try's deadline has passed.)
std::string starter_rng_outcome_name(StarterRngOutcome outcome);


//  Where the loop is, for an "RngPlan: state" line.
struct RngLoopState{
    //  From 1, of `attempts`.
    size_t attempt = 1;
    size_t attempts = 0;
    //  From 1, of `tries`.
    size_t try_number = 1;
    size_t tries = 0;
    uint64_t resets = 0;
    uint64_t max_resets = 0;
    uint16_t seed = 0;
    uint64_t advances = 0;
    //  The SIDs the attempt is shiny for, and those ruled out so far.
    std::vector<uint16_t> sids;
    std::vector<uint16_t> ruled_out;
    Milliseconds attempt_time = Milliseconds(0);
    Milliseconds attempt_cap = Milliseconds(0);
    Milliseconds total_time = Milliseconds(0);
    Milliseconds total_cap = Milliseconds(0);
    //  The calibration of the last reset, in frames; none before the first.
    std::optional<RngCalibrations> calibration;
};
//  Formats loop state summary line for logging.
std::string rng_log_state(const RngLoopState& state);


//  A reset's reads, for its one line on the stream's log.
struct RngResetLine{
    //  "Starter RNG" or "Gift RNG".
    std::string_view program;
    uint64_t max_resets = 0;
    uint16_t target_seed = 0;
    uint64_t target_advances = 0;
    //  The seeds near the target, in the order of their delays, and the
    //  target's place among them (SeedMatch).
    std::vector<uint16_t> seed_values;
    int16_t seed_position = -1;
    RngResetRead read;
};
//  The frames from the target seed to `seed` among `seed_values`: +1 for
//  the seed a frame later. None when either isn't among them.
std::optional<int> seed_frame_offset(const std::vector<uint16_t>& seed_values, int16_t target_position, uint16_t seed);
//  As "Gift RNG try 28/50: seed 9cf3 = target, advance 1669 (target 1670, -1). Missed."
std::string rng_reset_line(const RngResetLine& line);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
