/*  AutoStory RNG Loop
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryRngLoop.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


RngAttemptOutcome plan_outcome(StarterRngOutcome outcome){
    switch (outcome){
    case StarterRngOutcome::shiny:
        return RngAttemptOutcome::SHINY;
    case StarterRngOutcome::target_hit:
        return RngAttemptOutcome::HIT_NOT_SHINY;
    case StarterRngOutcome::wild_shiny:
    case StarterRngOutcome::out_of_resets:
    case StarterRngOutcome::no_matches:
        return RngAttemptOutcome::MISSED;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Starter RNG outcome.");
}


namespace{

std::string join_sids(const std::vector<uint16_t>& sids){
    std::string text;
    for (uint16_t sid : sids){
        text += (text.empty() ? "" : ",") + std::to_string(sid);
    }
    return text;
}

std::optional<StarterRngLoopEnd> loop_end(RngPlanStep step){
    switch (step){
    case RngPlanStep::RUN_ATTEMPT:
        return std::nullopt;
    case RngPlanStep::GOT_SHINY:
        return StarterRngLoopEnd::SHINY;
    case RngPlanStep::GOT_TARGET:
        return StarterRngLoopEnd::TARGET;
    case RngPlanStep::OUT_OF_ATTEMPTS:
        return StarterRngLoopEnd::OUT_OF_ATTEMPTS;
    case RngPlanStep::OUT_OF_RETRIES:
        return StarterRngLoopEnd::OUT_OF_RETRIES;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown RNG plan step.");
}

}


StarterRngLoopResult run_starter_rng_plan(
    Logger& logger, RngPlanTracker& tracker, StarterRngLoopSteps& steps,
    const StarterRngLoopLimits& limits, uint16_t tid, const std::vector<SidCandidate>& sid_helper_candidates
){
    StarterRngLoopResult result;
    const WallClock total_deadline = steps.now() + limits.total_time;
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
        const RngAttemptOutcome outcome = plan_outcome(steps.run_attempt(index, tracker.current_attempt(), deadline));
        result.attempts_run++;
        logger.log(rng_log_result(index, outcome));
        tracker.record(outcome);
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
        return result.sid
            ? std::format("Starter RNG: shiny Squirtle. The SID is {}: enter it as the RNG plan's SID.", *result.sid)
            : std::format("Starter RNG: shiny Squirtle. The SID is one of {}.", sids);
    case StarterRngLoopEnd::TARGET:
        return "Starter RNG hit the target (shiny not required). Nothing was saved after it: the game is where "
            "Starter RNG's calibration left it. Play to the rival's battle and save by hand, then start AutoStory at 00.05.";
    case StarterRngLoopEnd::OUT_OF_ATTEMPTS:
        return std::format(
            "Starter RNG: every planned attempt hit its target and none was shiny. SIDs left: {}. "
            "The save is still south of the table: raise the RNG plan's max attempts and start AutoStory at 00.04 again.", sids
        );
    case StarterRngLoopEnd::OUT_OF_RETRIES:
        return "Starter RNG: one attempt missed its target more often than the retries allow. "
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

StarterRngOutcome ConsoleStarterRngSteps::run_attempt(size_t index, const RngAttempt& attempt, WallClock deadline){
    //  Starter RNG's default Nearby Seed Radius.
    const int SEED_RADIUS = 5;
    const SeedMatch seed_match = m_seeds.find_seed(attempt.target.seed.seed, m_settings.sound, SEED_RADIUS);
    if (!seed_match.found){
        throw InternalProgramError(&m_env.logger(), PA_CURRENT_FUNCTION, "The plan's seed isn't in the seed list it came from.");
    }
    m_env.log(std::format("Starter RNG attempt {}: seed {:04X}, advances {}.", index + 1, attempt.target.seed.seed, attempt.target.advances));

    const uint64_t max_resets = m_settings.max_resets;
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
            .max_resets = [deadline, max_resets]{
                return current_time() >= deadline ? uint64_t(0) : max_resets;
            },
        },
        StarterRngDisplays{
            .target = m_target_display,
            .filters = m_filter_display,
            .calibration = m_calibration_display,
        },
        m_notifications,
        stats
    );
    return attempts.run(m_env, m_context).outcome;
}



}
}
}
