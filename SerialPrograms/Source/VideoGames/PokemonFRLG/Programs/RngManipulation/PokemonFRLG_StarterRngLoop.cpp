/*  Starter RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cstdlib>
#include <format>
#include <set>
#include <utility>
#include "Common/Cpp/Exceptions.h"
#include "VideoGames/PokemonFRLG/Programs/RngManipulation/PokemonFRLG_RngPlanLog.h"
#include "PokemonFRLG_StarterRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


RngAttemptOutcome plan_outcome(StarterRngOutcome outcome){
    switch (outcome){
    case StarterRngOutcome::shiny_on_target:
        return RngAttemptOutcome::SHINY;
    case StarterRngOutcome::shiny:
    case StarterRngOutcome::shiny_off_target:
        return RngAttemptOutcome::SHINY_OFF_TARGET;
    case StarterRngOutcome::target_hit:
        return RngAttemptOutcome::HIT_NOT_SHINY;
    case StarterRngOutcome::wild_shiny:
    case StarterRngOutcome::out_of_resets:
    case StarterRngOutcome::no_matches:
        return RngAttemptOutcome::MISSED;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Starter RNG outcome.");
}

std::string starter_rng_outcome_name(StarterRngOutcome outcome){
    switch (outcome){
    case StarterRngOutcome::shiny:
        return "shiny";
    case StarterRngOutcome::shiny_on_target:
        return "shiny_on_target";
    case StarterRngOutcome::shiny_off_target:
        return "shiny_off_target";
    case StarterRngOutcome::target_hit:
        return "target_hit";
    case StarterRngOutcome::wild_shiny:
        return "wild_shiny";
    case StarterRngOutcome::out_of_resets:
        return "out_of_resets";
    case StarterRngOutcome::no_matches:
        return "no_matches";
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Starter RNG outcome.");
}


namespace{

//  "-" for none.
std::string join_sids(const std::vector<uint16_t>& sids){
    std::string text;
    for (uint16_t sid : sids){
        text += (text.empty() ? "" : ",") + std::to_string(sid);
    }
    return text.empty() ? "-" : text;
}

std::string clock_text(Milliseconds time){
    const int64_t seconds = std::chrono::duration_cast<std::chrono::seconds>(time).count();
    return std::format("{:02}:{:02}:{:02}", seconds / 3600, seconds / 60 % 60, seconds % 60);
}

std::string calibration_text(const std::optional<RngCalibrations>& calibration){
    if (!calibration){
        return "-";
    }
    return std::format("{:.2f}/{:.2f}/{:.2f}", calibration->seed_offset, calibration->csf_offset, calibration->ingame_offset);
}

} // namespace

std::string rng_log_state(const RngLoopState& state){
    return std::format(
        "RngPlan: state attempt={}/{} try={}/{} reset={}/{} seed={:04X} advances={} sids={} ruled_out={} "
        "attempt_time={}/{} total_time={}/{} calibration={}",
        state.attempt, state.attempts, state.try_number, state.tries, state.resets, state.max_resets,
        state.seed, state.advances, join_sids(state.sids), join_sids(state.ruled_out),
        clock_text(state.attempt_time), clock_text(state.attempt_cap),
        clock_text(state.total_time), clock_text(state.total_cap),
        calibration_text(state.calibration)
    );
}


std::optional<int> seed_frame_offset(const std::vector<uint16_t>& seed_values, int16_t target_position, uint16_t seed){
    const auto found = std::find(seed_values.begin(), seed_values.end(), seed);
    if (target_position < 0 || found == seed_values.end()){
        return std::nullopt;
    }
    return int(found - seed_values.begin()) - target_position;
}

namespace{

std::string seed_text(const RngResetLine& line, uint16_t seed){
    if (seed == line.target_seed){
        return std::format("seed {:04x} = target", seed);
    }
    const std::optional<int> frames = seed_frame_offset(line.seed_values, line.seed_position, seed);
    if (!frames){
        return std::format("seed {:04x} (not near the target)", seed);
    }
    return std::format(
        "seed {:04x} ({:+} frame{} {})", seed, *frames, std::abs(*frames) == 1 ? "" : "s", *frames > 0 ? "late" : "early"
    );
}

std::string advance_text(uint64_t target, uint64_t advance){
    if (advance == target){
        return std::format("advance {}", advance);
    }
    return std::format("advance {} (target {}, {:+})", advance, target, int64_t(advance) - int64_t(target));
}

size_t distinct_states(const std::vector<AdvRngState>& hits){
    std::set<std::pair<uint16_t, uint64_t>> states;
    for (const AdvRngState& hit : hits){
        states.emplace(hit.seed, hit.advance);
    }
    return states.size();
}

std::string shiny_text(const RngResetLine& line){
    const std::vector<AdvRngState>& hits = line.read.hits;
    const bool on_target = std::any_of(hits.begin(), hits.end(), [&](const AdvRngState& hit){
        return have_hit_target(line.target_seed, line.target_advances, hit);
    });
    if (on_target){
        return seed_text(line, line.target_seed) + ", " + advance_text(line.target_advances, line.target_advances) + ". HIT, shiny!";
    }
    return hits.empty() ? "shiny, not known if on the target." : "shiny, not on the target.";
}

} // namespace

std::string rng_reset_line(const RngResetLine& line){
    const RngResetRead& read = line.read;
    const std::string head = std::format("{} try {}/{}: ", line.program, read.resets, line.max_resets);
    if (read.shiny){
        return head + shiny_text(line);
    }
    const size_t states = distinct_states(read.hits);
    std::optional<AdvRngState> hit = read.hit;
    if (!hit && states == 1){
        hit = read.hits.front();
    }
    if (!hit){
        return head + (states == 0
            ? std::string("no match read (stats fit no seed/advance).")
            : std::format("{} possible matches, can't tell which.", states));
    }
    const bool on_target = have_hit_target(line.target_seed, line.target_advances, *hit);
    return head + seed_text(line, hit->seed) + ", " + advance_text(line.target_advances, hit->advance)
        + (on_target ? ". HIT, not shiny." : ". Missed.");
}


namespace{

std::optional<StarterRngLoopEnd> loop_end(RngPlanStep step){
    switch (step){
    case RngPlanStep::RUN_ATTEMPT:
        return std::nullopt;
    case RngPlanStep::GOT_SHINY:
        return StarterRngLoopEnd::SHINY;
    case RngPlanStep::GOT_SHINY_OFF_TARGET:
        return StarterRngLoopEnd::SHINY_OFF_TARGET;
    case RngPlanStep::GOT_TARGET:
        return StarterRngLoopEnd::TARGET;
    case RngPlanStep::OUT_OF_ATTEMPTS:
        return StarterRngLoopEnd::OUT_OF_ATTEMPTS;
    case RngPlanStep::MISSED_EVERY_ATTEMPT_LEFT:
        return StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RNG plan step.");
}

//  The loop's state lines and tries, across the attempts.
class LoopLog{
public:
    LoopLog(Logger& logger, const RngPlanTracker& tracker, const StarterRngLoopLimits& limits, WallClock start)
        : m_logger(logger)
        , m_tracker(tracker)
        , m_limits(limits)
        , m_start(start)
        , m_start_sids(tracker.remaining_sids())
    {}

    //  A new try of the tracker's current attempt, starting at `now`.
    void begin_try(WallClock now){
        const size_t index = m_tracker.attempt_index();
        m_try = (m_tries_of && *m_tries_of == index) ? m_try + 1 : 1;
        m_tries_of = index;
        m_try_start = now;
    }
    size_t try_number() const{
        return m_try;
    }

    void log_state(WallClock now, uint64_t resets, const std::optional<RngCalibrations>& calibration){
        const RngAttempt& attempt = m_tracker.current_attempt();
        std::vector<uint16_t> ruled_out;
        for (uint16_t sid : m_start_sids){
            const std::vector<uint16_t>& left = m_tracker.remaining_sids();
            if (std::find(left.begin(), left.end(), sid) == left.end()){
                ruled_out.push_back(sid);
            }
        }
        m_logger.log(rng_log_state(RngLoopState{
            .attempt = m_tracker.attempt_index() + 1, .attempts = m_tracker.attempt_count(),
            .try_number = m_try, .tries = m_limits.max_retries + 1,
            .resets = resets, .max_resets = m_limits.max_resets,
            .seed = attempt.target.seed.seed, .advances = attempt.target.advances,
            .sids = attempt.sids, .ruled_out = std::move(ruled_out),
            .attempt_time = std::chrono::duration_cast<Milliseconds>(now - m_try_start), .attempt_cap = m_limits.attempt_time,
            .total_time = std::chrono::duration_cast<Milliseconds>(now - m_start), .total_cap = m_limits.total_time,
            .calibration = calibration,
        }));
    }

private:
    Logger& m_logger;
    const RngPlanTracker& m_tracker;
    const StarterRngLoopLimits& m_limits;
    const WallClock m_start;
    const std::vector<uint16_t> m_start_sids;
    std::optional<size_t> m_tries_of;
    size_t m_try = 0;
    WallClock m_try_start;
};

//  What one try came to, for the tracker and the result line.
struct TryOutcome{
    RngAttemptOutcome outcome = RngAttemptOutcome::MISSED;
    std::string detail;
    std::optional<AdvPokemonResult> pokemon;
};

//  Runs one try of the tracker's current attempt, logging its state lines.
StarterRngResult run_try(
    LoopLog& log, const RngPlanTracker& tracker, StarterRngLoopSteps& steps,
    const StarterRngLoopLimits& limits, WallClock start, WallClock deadline
){
    log.begin_try(start);
    log.log_state(start, 0, std::nullopt);
    StarterRngAttemptCall call{
        .index = tracker.attempt_index(),
        .deadline = deadline,
        .max_resets = limits.max_resets,
        .on_reset = [&](const StarterRngProgress& progress){
            log.log_state(steps.now(), progress.resets, progress.calibrations);
        },
    };
    return steps.run_attempt(tracker.current_attempt(), call);
}

//  run_try(), with Starter RNG refusing the attempt's timings as an attempt
//  that can't run. Other errors, a setup error such as no sound included,
//  would fail every attempt the same way, so they end the loop.
TryOutcome run_try_outcome(
    Logger& logger, LoopLog& log, const RngPlanTracker& tracker, StarterRngLoopSteps& steps,
    const StarterRngLoopLimits& limits, WallClock start, WallClock deadline
){
    try{
        const StarterRngResult attempt = run_try(log, tracker, steps, limits, start, deadline);
        //  The deadline ends a try as out of resets.
        const bool past_deadline = attempt.outcome == StarterRngOutcome::out_of_resets && steps.now() >= deadline;
        return TryOutcome{
            .outcome = plan_outcome(attempt.outcome),
            .detail = past_deadline ? "attempt_deadline" : starter_rng_outcome_name(attempt.outcome),
            .pokemon = attempt.pokemon,
        };
    }catch (StarterRngTimingsRefused& e){
        logger.log(std::format("RngPlan: attempt {} can't run: {}", tracker.attempt_index() + 1, e.message()), COLOR_RED);
        return TryOutcome{.outcome = RngAttemptOutcome::UNUSABLE, .detail = "setup_error", .pokemon = std::nullopt};
    }
}

} // namespace


StarterRngLoopResult run_starter_rng_plan(
    Logger& logger, RngPlanTracker& tracker, StarterRngLoopSteps& steps,
    const StarterRngLoopLimits& limits, uint16_t tid, const std::vector<SidCandidate>& sid_helper_candidates
){
    StarterRngLoopResult result;
    const WallClock loop_start = steps.now();
    const WallClock total_deadline = loop_start + limits.total_time;
    LoopLog log(logger, tracker, limits, loop_start);
    while (true){
        std::optional<StarterRngLoopEnd> end = loop_end(tracker.step());
        if (end){
            result.end = *end;
            break;
        }
        const WallClock start = steps.now();
        if (start >= total_deadline){
            logger.log("RngPlan: out of time before attempt " + std::to_string(tracker.attempt_index() + 1));
            result.end = StarterRngLoopEnd::OUT_OF_TIME;
            break;
        }
        const size_t index = tracker.attempt_index();
        const WallClock deadline = std::min(start + limits.attempt_time, total_deadline);
        const TryOutcome attempt = run_try_outcome(logger, log, tracker, steps, limits, start, deadline);
        result.attempts_run++;
        const RngPlanStep step = tracker.record(attempt.outcome);
        logger.log(rng_log_result(RngResultLog{
            .index = index, .try_number = log.try_number(), .outcome = attempt.outcome,
            .detail = attempt.detail, .pokemon = attempt.pokemon,
            .next = rng_plan_next(step, index, tracker.attempt_index()),
        }));
    }
    result.remaining_sids = tracker.remaining_sids();
    result.sid = tracker.confirmed_sid();
    if (result.end == StarterRngLoopEnd::SHINY && result.sid){
        logger.log(rng_log_confirmed(tid, *result.sid, sid_helper_candidates));
    }
    return result;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
