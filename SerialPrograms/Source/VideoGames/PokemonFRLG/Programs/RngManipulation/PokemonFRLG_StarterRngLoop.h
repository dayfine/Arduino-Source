/*  Starter RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  An RNG plan's attempts (PokemonFRLG_RngPlan.h), each run with Starter
 *  RNG's attempt loop (StarterRngAttempts), until one comes out shiny or
 *  the plan, its retries or the time run out. Its "RngPlan: state" and
 *  "RngPlan: result" lines are listed with the plan's in
 *  PokemonFRLG_RngPlanLog.h.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngLoop_H
#define PokemonAutomation_PokemonFRLG_StarterRngLoop_H

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Exceptions.h"
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/Time.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngCalibration.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlan.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanTracker.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngTargetSearch.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_StarterRngAttempts.h"

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
std::string rng_log_state(const RngLoopState& state);


struct StarterRngLoopLimits{
    //  An attempt makes no more resets after this long, and counts as missed.
    Milliseconds attempt_time = Milliseconds(0);
    //  No attempt starts after this long.
    Milliseconds total_time = Milliseconds(0);
    //  Resets per try of an attempt.
    uint64_t max_resets = 0;
    //  The tracker's retries of a missed attempt, for the state lines.
    size_t max_retries = 0;
};

//  Starter RNG refused an attempt's timings (check_timings()). The loop
//  records the attempt as unusable and moves on; any other error, a
//  UserSetupError included, stops the loop.
class StarterRngTimingsRefused : public Exception{
public:
    explicit StarterRngTimingsRefused(std::string message) : m_message(std::move(message)) {}
    virtual const char* name() const override{ return "StarterRngTimingsRefused"; }
    virtual std::string message() const override{ return m_message; }

private:
    std::string m_message;
};

//  One try of an attempt, as the loop asks the steps for it.
struct StarterRngAttemptCall{
    //  From 0.
    size_t index = 0;
    //  No reset after this.
    WallClock deadline;
    uint64_t max_resets = 0;
    //  To call after each reset.
    std::function<void(const StarterRngProgress&)> on_reset;
};

//  The game steps of the loop, injected so that its decisions run in tests
//  without a Switch.
class StarterRngLoopSteps{
public:
    virtual ~StarterRngLoopSteps() = default;
    virtual WallClock now() = 0;
    //  Runs Starter RNG's attempts at `attempt`'s target from the save under
    //  the ball, as `call` says. Throws StarterRngTimingsRefused when
    //  Starter RNG refuses the attempt's timings; the loop logs it and moves on.
    virtual StarterRngResult run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call) = 0;
};

enum class StarterRngLoopEnd{
    //  The starter is shiny and the target; the game is on its summary, not saved.
    SHINY,
    //  The starter is shiny but not the target (or not known to be), so it
    //  doesn't tell the SID; the game is on its summary, not saved.
    SHINY_OFF_TARGET,
    //  Shiny not required, and the target was hit. The game is where
    //  Starter RNG's calibration left it, not saved.
    TARGET,
    //  Every attempt was hit and none was shiny.
    OUT_OF_ATTEMPTS,
    //  The last attempt still shiny for a SID left missed more often than
    //  the retries allow, or couldn't run.
    MISSED_EVERY_ATTEMPT_LEFT,
    //  The total time ran out with attempts left.
    OUT_OF_TIME,
};

struct StarterRngLoopResult{
    StarterRngLoopEnd end = StarterRngLoopEnd::OUT_OF_TIME;
    //  Attempts run, retries included.
    size_t attempts_run = 0;
    //  The SID candidates not ruled out.
    std::vector<uint16_t> remaining_sids;
    //  The SID, once a shiny leaves one candidate.
    std::optional<uint16_t> sid;
};

//  Runs `tracker`'s attempts through `steps` until the plan ends or
//  `limits.total_time` has passed. Logs an "RngPlan: state" line at the
//  start of each try and after each reset, an "RngPlan: result" line after
//  it and, when a shiny leaves one SID, an "RngPlan: confirmed" line placing
//  it among SID Helper's candidates for `tid`.
StarterRngLoopResult run_starter_rng_plan(
    Logger& logger, RngPlanTracker& tracker, StarterRngLoopSteps& steps,
    const StarterRngLoopLimits& limits, uint16_t tid, const std::vector<SidCandidate>& sid_helper_candidates
);



}
}
}
#endif
