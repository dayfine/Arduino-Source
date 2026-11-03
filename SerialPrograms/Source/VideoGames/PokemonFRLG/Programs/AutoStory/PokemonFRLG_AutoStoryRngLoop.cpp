/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"

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

}

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

}


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
        const StarterRngResult attempt = run_try(log, tracker, steps, limits, start, deadline);
        //  The deadline ends a try as out of resets.
        const bool past_deadline = attempt.outcome == StarterRngOutcome::out_of_resets && steps.now() >= deadline;
        const RngAttemptOutcome outcome = plan_outcome(attempt.outcome);
        result.attempts_run++;
        const RngPlanStep step = tracker.record(outcome);
        logger.log(rng_log_result(RngResultLog{
            .index = index, .try_number = log.try_number(), .outcome = outcome,
            .detail = past_deadline ? "attempt_deadline" : starter_rng_outcome_name(attempt.outcome), .pokemon = attempt.pokemon,
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

std::string starter_rng_loop_message(const StarterRngLoopResult& result){
    const std::string sids = join_sids(result.remaining_sids);
    switch (result.end){
    case StarterRngLoopEnd::SHINY:
        if (result.sid){
            return std::format("Starter RNG: shiny Squirtle. The SID is {}: enter it as the RNG plan's SID.", *result.sid);
        }
        if (result.remaining_sids.empty()){
            return "Starter RNG: shiny Squirtle, for none of the SID candidates left: the SID isn't one of SID Helper's candidates.";
        }
        return std::format("Starter RNG: shiny Squirtle. The SID is one of {}.", sids);
    case StarterRngLoopEnd::SHINY_OFF_TARGET:
        return std::format(
            "Starter RNG: shiny Squirtle, but not the planned target, so it doesn't tell the SID. SIDs left: {}.", sids
        );
    case StarterRngLoopEnd::TARGET:
        return "Starter RNG hit the target (shiny not required). Nothing was saved after it: the game is where "
            "Starter RNG's calibration left it. Play to the rival's battle and save by hand, then start AutoStory at 00.05.";
    case StarterRngLoopEnd::OUT_OF_ATTEMPTS:
        return std::format(
            "Starter RNG: every planned attempt hit its target and none was shiny. SIDs left: {}. "
            "The save is still south of the table: raise the RNG plan's max attempts and start AutoStory at 00.04 again.", sids
        );
    case StarterRngLoopEnd::MISSED_EVERY_ATTEMPT_LEFT:
        return "Starter RNG: every attempt left missed its target more often than the retries allow. "
            "The save is still south of the table: check the RNG plan's sound setting and start AutoStory at 00.04 again.";
    case StarterRngLoopEnd::OUT_OF_TIME:
        return std::format(
            "Starter RNG: out of time after {} attempts. The save is still south of the table: start AutoStory at 00.04 again.",
            result.attempts_run
        );
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Starter RNG loop end.");
}


std::vector<StoryCheckpoint> rows_after_starter_rng(
    const std::vector<StoryCheckpoint>& checkpoints, size_t ball_row, size_t last
){
    if (ball_row + 1 >= checkpoints.size()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "No checkpoint after the starter's ball.");
    }
    StoryCheckpoint row = checkpoints[ball_row + 1];
    auto taken = std::find_if(row.steps.begin(), row.steps.end(), [](const StoryStep& step){
        return step.name == "rival_takes_bulbasaur";
    });
    if (taken == row.steps.end()){
        throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, row.id + " has no rival_takes_bulbasaur step.");
    }
    row.steps.erase(row.steps.begin(), taken + 1);
    row.title = "after Starter RNG in AutoStory: the rival's battle";

    std::vector<StoryCheckpoint> rows{std::move(row)};
    if (last > ball_row + 1){
        StoryRunPlan rest = plan_story_run(checkpoints, ball_row + 2, last, StoryHandoffs{});
        rows.insert(rows.end(), rest.rows.begin(), rest.rows.end());
    }
    return rows;
}


RngPlanLogger::RngPlanLogger(ConsoleHandle& console)
    : m_console(console)
{}

void RngPlanLogger::log(const std::string& msg, Color color){
    m_console.log(msg, color);
    if (msg.starts_with("RngPlan: state") || msg.starts_with("RngPlan: result")){
        m_console.overlay().add_log(msg);
    }
}


ConsoleStarterRngSteps::ConsoleStarterRngSteps(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const StarterRngConsoleSettings& settings, const StarterRngNotifications& notifications
)
    : m_env(env)
    , m_context(context)
    , m_settings(settings)
    , m_notifications(notifications)
    //  TODO: LeafGreen, once AutoStory plays it.
    , m_seeds(seeds_json_path(true, settings.language))
{}

WallClock ConsoleStarterRngSteps::now(){
    return current_time();
}

StarterRngResult ConsoleStarterRngSteps::run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call){
    //  Starter RNG's default Nearby Seed Radius.
    const int SEED_RADIUS = 5;
    const SeedMatch seed_match = m_seeds.find_seed(attempt.target.seed.seed, m_settings.sound, SEED_RADIUS);
    if (!seed_match.found){
        throw InternalProgramError(&m_env.logger(), PA_CURRENT_FUNCTION, "The plan's seed isn't in the seed list it came from.");
    }

    StarterRngStats stats;
    StarterRngAttempts attempts(
        StarterRngTarget{
            .starter = StarterSpecies::squirtle,
            .seed = attempt.target.seed.seed,
            .advances = attempt.target.advances,
            .seed_match = seed_match,
        },
        StarterRngSettings{
            .language = m_settings.language,
            .profile = 0,
            .ignore_wild_shinies = true,
            .take_video = false,
            .identify_shiny = true,
            .max_resets = [deadline = call.deadline, max_resets = call.max_resets]{
                return current_time() >= deadline ? uint64_t(0) : max_resets;
            },
            .on_reset = call.on_reset,
        },
        StarterRngDisplays{
            .target = m_target_display,
            .filters = m_filter_display,
            .calibration = m_calibration_display,
        },
        m_notifications,
        stats
    );
    return attempts.run(m_env, m_context);
}



}
}
}
