/*  Starter RNG Console Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "PokemonFRLG_StarterRngConsoleSteps.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


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
{
    m_calibration_display.set_calibrations(settings.calibration);
}

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
    try{
        return attempts.run(m_env, m_context);
    }catch (OperationFailedException& e){
        //  Starter RNG refuses timings it can't press (check_timings()) this
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
