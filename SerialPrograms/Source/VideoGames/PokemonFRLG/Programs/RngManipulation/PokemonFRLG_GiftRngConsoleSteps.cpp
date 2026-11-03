/*  Gift RNG Console Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Exceptions.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "PokemonFRLG_GiftRngConsoleSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


StarterRngOutcome loop_outcome(GiftRngOutcome outcome){
    switch (outcome){
    case GiftRngOutcome::shiny:
        return StarterRngOutcome::shiny;
    case GiftRngOutcome::shiny_on_target:
        return StarterRngOutcome::shiny_on_target;
    case GiftRngOutcome::shiny_off_target:
        return StarterRngOutcome::shiny_off_target;
    case GiftRngOutcome::target_hit:
        return StarterRngOutcome::target_hit;
    case GiftRngOutcome::out_of_resets:
        return StarterRngOutcome::out_of_resets;
    case GiftRngOutcome::no_matches:
        return StarterRngOutcome::no_matches;
    }
    throw InternalProgramError(nullptr, PA_CURRENT_FUNCTION, "Unknown Gift RNG outcome.");
}


namespace{

//  Gift RNG's settings for one try: no resets after the try's deadline,
//  and its resets reported to the loop.
GiftRngSettings try_settings(Language language, const StarterRngAttemptCall& call){
    return GiftRngSettings{
        .language = language,
        .profile = 0,
        .use_teachy_tv = false,
        .take_video = false,
        .identify_shiny = true,
        .max_resets = [deadline = call.deadline, max_resets = call.max_resets]{
            return current_time() >= deadline ? uint64_t(0) : max_resets;
        },
        .max_rare_candies = []{ return uint64_t(0); },
        .on_reset = [on_reset = call.on_reset](const GiftRngProgress& progress){
            if (on_reset){
                on_reset(StarterRngProgress{.resets = progress.resets, .calibrations = progress.calibrations});
            }
        },
    };
}

}


ConsoleGiftRngSteps::ConsoleGiftRngSteps(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    const GiftRngConsoleSettings& settings, const GiftRngNotifications& notifications
)
    : m_env(env)
    , m_context(context)
    , m_settings(settings)
    , m_notifications(notifications)
    //  TODO: LeafGreen, once AutoStory plays it.
    , m_seeds(seeds_json_path(true, settings.language))
{
    m_calibration_display.set_calibrations(settings.calibration);
}

WallClock ConsoleGiftRngSteps::now(){
    return current_time();
}

StarterRngResult ConsoleGiftRngSteps::run_attempt(const RngAttempt& attempt, const StarterRngAttemptCall& call){
    //  Gift RNG's default Nearby Seed Radius.
    const int SEED_RADIUS = 5;
    const SeedMatch seed_match = m_seeds.find_seed(attempt.target.seed.seed, m_settings.sound, SEED_RADIUS);
    if (!seed_match.found){
        throw InternalProgramError(&m_env.logger(), PA_CURRENT_FUNCTION, "The plan's seed isn't in the seed list it came from.");
    }

    GiftRngStats stats;
    GiftRngAttempts attempts(
        GiftRngTarget{
            .target = PokemonFRLG_RngTarget::magikarp,
            .seed = attempt.target.seed.seed,
            .advances = attempt.target.advances,
            .seed_match = seed_match,
        },
        try_settings(m_settings.language, call),
        GiftRngDisplays{
            .target = m_target_display,
            .filters = m_filter_display,
            .calibration = m_calibration_display,
        },
        m_notifications,
        stats
    );
    try{
        const GiftRngResult result = attempts.run(m_env, m_context);
        return StarterRngResult{.outcome = loop_outcome(result.outcome), .pokemon = result.pokemon};
    }catch (OperationFailedException& e){
        //  Gift RNG refuses timings it can't press (check_timings()) this
        //  way; a failure in the game sends an error report.
        if (e.error_report_mode() != ErrorReportMode::NO_ERROR_REPORT){
            throw;
        }
        throw StarterRngTimingsRefused(e.message());
    }
}



}
}
}
