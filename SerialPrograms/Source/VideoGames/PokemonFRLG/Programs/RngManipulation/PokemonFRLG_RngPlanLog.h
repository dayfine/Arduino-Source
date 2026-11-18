/*  RNG Plan Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The lines an RNG plan logs and shows: its attempts, and the "RngPlan:"
 *  lines below.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngPlanLog_H
#define PokemonAutomation_PokemonFRLG_RngPlanLog_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_AdvRng.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  One line per attempt, for the log and the stop message: the Target Seed
//  and Advances to give Starter RNG, the Pokemon, and the SIDs it is shiny for.
std::vector<std::string> rng_attempt_lines(const std::vector<RngAttempt>& attempts);


//  The data to rebuild how likely SID Helper's k-th candidate of N is (the
//  1-2-3-2-1 weights of sid_helper_candidates() are a guess), from the logs
//  alone, and to follow AutoStory's Starter RNG loop. Every line starts with
//  "RngPlan:"; grep for it. In the order a run logs them:
//      RngPlan: tid=<TID> source=<card|option>
//      RngPlan: sid_helper tid=<TID> target_advances=<A> candidates=<SID>,...
//      RngPlan: attempt=<i> seed=<hex> advances=<n> sids=<SID>,...
//          seed_wait_ms=<ms> ingame_ms=<ms>   (Starter RNG's waits, before calibration)
//  then, with Run Starter RNG in AutoStory, for each try of an attempt
//  (rng_log_state() in PokemonFRLG_StarterRngLoop.h):
//      RngPlan: state attempt=<i>/<n> try=<t>/<tries> reset=<r>/<max> seed=<hex>
//          advances=<n> sids=<SID>,... ruled_out=<SID>,... attempt_time=<h:mm:ss>/<cap>
//          total_time=<h:mm:ss>/<cap> calibration=<seed>/<csf>/<ingame frames|->
//      (once at the try's start, then after every reset)
//      RngPlan: result attempt=<i> try=<t> outcome=<shiny|shiny_off_target|hit_not_shiny|missed|unusable>
//          starter_rng=<Starter RNG's outcome> nature=<N|-> ivs=<a/b/c/d/e/f|->
//          pid=<hex|-> next=<retry|next_attempt|confirm|stop>
//      RngPlan: confirmed tid=<TID> sid=<SID> candidate=<k> of=<N>
//  Lists that are empty print "-".
//  "candidates" are in SID Helper's order; "candidate=k" counts from 1 in
//  it, 0 when the SID wasn't among them. Count each TID's "confirmed" once:
//  every later run with the SID set logs it again. While the user runs
//  Starter RNG by hand, its own "Target Hit!" and "Missed target." lines
//  are the attempts' results.
std::string rng_log_tid(uint16_t tid, bool from_card);
//  Formats SID Helper summary line for log output.
std::string rng_log_sid_helper(uint16_t tid, uint32_t target_advances, const std::vector<SidCandidate>& candidates);
//  Formats attempt parameters for log output.
std::string rng_log_attempt(size_t index, const RngAttempt& attempt);

//  Structured log summary of an RNG outcome.
struct RngResultLog{
    size_t index = 0;
    //  From 1.
    size_t try_number = 1;
    RngAttemptOutcome outcome = RngAttemptOutcome::MISSED;
    //  How the RNG program put it ("out_of_resets"); empty for none.
    std::string detail;
    //  The Pokemon hit, when known.
    std::optional<Pokemon::AdvPokemonResult> pokemon;
    RngPlanNext next = RngPlanNext::STOP;
};
//  Formats RNG attempt result for logging.
std::string rng_log_result(const RngResultLog& result);
//  Formats confirmed SID and candidate weights for logging.
std::string rng_log_confirmed(uint16_t tid, uint16_t sid, const std::vector<SidCandidate>& sid_helper_candidates);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
