/*  BDSP AutoStory: Starter RNG
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <memory>
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/VideoPipeline/VideoOverlay.h"
#include "PokemonBDSP_AutoStory_StarterRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


BdspStarter bdsp_starter_for_choice(StarterChoice choice){
    switch (choice){
    case StarterChoice::TURTWIG:
        return BdspStarter::Turtwig;
    case StarterChoice::CHIMCHAR:
        return BdspStarter::Chimchar;
    case StarterChoice::PIPLUP:
        return BdspStarter::Piplup;
    }
    return BdspStarter::Chimchar;
}


StoryStarterRngOptions::StoryStarterRngOptions()
    : RUN(
        "<b>Run Starter RNG in AutoStory:</b><br>Off by default. In 00.02, run Starter RNG from the Verity Lakefront "
        "save using the stop conditions below, then continue the story at 00.03. "
        "Takes precedence over Stop for Starter RNG. If it fails the program stops: it does not reload 00.02.",
        LockMode::LOCK_WHILE_RUNNING,
        false
    )
    , MAX_RESETS(
        "<b>Starter RNG Maximum Resets:</b><br>0 means no limit.",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 9999
    )
    , FILTERS(
        "<b>Starter RNG Stop Conditions:</b><br>Stop Starter RNG when the starter matches any row in this table.",
        Pokemon::StatsHuntMiscFeatureFlags{
            /*action*/ false, /*shiny*/ true, /*gender*/ true, /*nature*/ true
        }
    )
    , NOTIFICATION_SHINY(
        "Shiny Starter",
        true, true, ImageAttachmentMode::JPG,
        {"Notifs", "Showcase"}
    )
{
    set_default_shiny_row(FILTERS);
}

void StoryStarterRngOptions::set_details_visible(bool visible){
    const ConfigOptionState state = visible ? ConfigOptionState::ENABLED : ConfigOptionState::HIDDEN;
    PLAYER_MODEL.set_visibility(state);
    MAX_RESETS.set_visibility(state);
    FILTERS.set_visibility(state);
}

StoryStarterRngConfig StoryStarterRngOptions::config(Language language, StarterChoice starter) const{
    return StoryStarterRngConfig{
        .language = language,
        .player_model = PLAYER_MODEL.model_number(),
        .starter = starter,
        .max_resets = MAX_RESETS,
    };
}


StarterRngSettings make_starter_rng_settings(const StoryStarterRngConfig& config, const BdspRngFilterSnapshot& filters){
    StarterRngSettings settings;
    settings.language = config.language;
    settings.player_model = config.player_model;
    settings.start_point = BdspStartPoint::Lakefront;
    settings.starter = bdsp_starter_for_choice(config.starter);
    settings.filters = &filters;
    settings.max_resets = config.max_resets;
    settings.reset_first = false;
    settings.finish_battle_on_shiny = true;
    settings.close_summary_after_read = true;
    return settings;
}

std::string starter_rng_failure(const StarterRngRunResult& result){
    switch (result.outcome){
    case StarterRngOutcome::HIT:
        return "";
    case StarterRngOutcome::SHINY:
        if (!result.settled){
            return "Caught a shiny starter, but the Starly battle or Rowan's scene did not finish: "
                "stopped with the shiny in the party; the game is not on the Verity Lakefront overworld.";
        }
        return "";
    case StarterRngOutcome::OUT_OF_RESETS:
    case StarterRngOutcome::ABANDONED:
        break;
    }
    if (result.last_failure_reason.empty()){
        return std::format("Starter RNG stopped after {} reset(s) without hitting a target.", result.resets_used);
    }
    return result.last_failure_reason;
}


namespace{

//  What the hook keeps between its calls: the filters, copied once so that
//  edits made while running do not apply, and the aim that Starter RNG's
//  calibration builds up across them.
struct StarterRngHookState{
    StarterRngHookState(const StoryStarterRngOptions& options, const StoryStarterRngConfig& config)
        : filters(options.filters().make_snapshot())
        , settings(make_starter_rng_settings(config, filters))
    {}

    RngAim aim;
    BdspRngFilterSnapshot filters;
    StarterRngSettings settings;
};

} // namespace


std::function<Pokemon::Outcome()> make_starter_rng_hook(
    SingleSwitchProgramEnvironment& env, ProControllerContext& context,
    StoryStarterRngOptions& options, const StoryStarterRngConfig& config,
    EventNotificationOption& error_notification
){
    if (options.filters().current_rows() == 0){
        throw UserSetupError(
            env.console,
            "Starter RNG Stop Conditions is empty, so no starter would ever match: add a row, such as a shiny of any nature."
        );
    }
    std::shared_ptr<StarterRngHookState> state = std::make_shared<StarterRngHookState>(options, config);
    return [&env, &context, &options, &error_notification, state]() -> Pokemon::Outcome{
        uint64_t resets_logged = 0;
        StarterRngAttemptIO io;
        io.notification_shiny = &options.notification_shiny();
        io.notification_error = &error_notification;
        io.on_reset = [&]{
            resets_logged++;
            env.console.overlay().add_log(std::format("Starter RNG: try {}", resets_logged));
        };
        const StarterRngRunResult result = run_starter_rng_attempts(env, context, state->settings, io, state->aim);
        const std::string failure = starter_rng_failure(result);
        if (!failure.empty()){
            throw_and_log<OperationFailedException>(env.logger(), ErrorReportMode::SEND_ERROR_REPORT, failure);
        }
        env.log(
            std::format(
                "Starter RNG succeeded after {} reset(s) on advance {}: {}.",
                result.resets_used, result.target_advance, result.target.to_string()
            ),
            COLOR_BLUE
        );
        return Pokemon::outcome_ok();
    };
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
