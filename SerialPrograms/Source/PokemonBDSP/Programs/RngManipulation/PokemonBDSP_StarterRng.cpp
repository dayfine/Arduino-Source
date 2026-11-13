/*  BDSP Starter RNG
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <memory>
#include <vector>
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"
#include "Pokemon/Options/Pokemon_StatsHuntFilter.h"
#include "Pokemon/Pokemon_Strings.h"
#include "PokemonBDSP/Inference/Rng/PokemonBDSP_SummaryReader.h"
#include "PokemonBDSP_StarterRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace Pokemon;


const size_t CALIBRATION_MIN_SAMPLES = 3;
const double CALIBRATION_THRESHOLD = 1.0;


StarterRng_Descriptor::StarterRng_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonBDSP:StarterRng",
        STRING_POKEMON + " BDSP", "Starter RNG",
        "",
        "Starter RNG manipulation using blinks.",
        PanelQualityStanding::OFFICIAL_BETA,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> StarterRng_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}


StarterRng::StarterRng()
    : m_aim(CALIBRATION_MIN_SAMPLES, CALIBRATION_THRESHOLD)
    , LANGUAGE(
        "<b>Game Language:</b>",
        summary_nature_languages(),
        LockMode::LOCK_WHILE_RUNNING, true
    )
    , START_POINT(
        "<b>Start point:</b><br>"
        "Where you have saved your game. Starting from the bedroom allows the possibility to skip advances quickly.",
        {
            {BdspStartPoint::Lakefront, "lakefront", "Verity Lakefront"},
            {BdspStartPoint::Bedroom,   "bedroom",   "Bedroom"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        BdspStartPoint::Lakefront
    )
    , STARTER(
        "<b>Starter:</b><br>",
        {
            {BdspStarter::Turtwig,  "turtwig",  "Turtwig"},
            {BdspStarter::Chimchar, "chimchar", "Chimchar"},
            {BdspStarter::Piplup,   "piplup",   "Piplup"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        BdspStarter::Turtwig
    )
    , FILTERS(
        "<b>Stop Conditions:</b><br>"
        "If the " + STRING_POKEMON + " matches any one of these filters, the program will "
        "stop.<br>",
        StatsHuntMiscFeatureFlags{
            /*action*/ false, /*shiny*/ true, /*gender*/ true, /*nature*/ true
        }
    )
    , COLLECTION_DISPLAY(true)
    , MAX_RESETS(
        "<b>Maximum resets:</b><br>"
        "Set this to zero to keep going until a shiny is caught.",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 1000
    )
    , MAX_TARGET_WAIT_MINUTES(
        "<b>Wait at most (minutes) for a target:</b><br>"
        "Time limit for waiting at Verity Lake. Rarer targets will require longer "
        "wait times.",
        LockMode::LOCK_WHILE_RUNNING,
        30, 5, 1440
    )
    , MAX_SKIP_MINUTES(
        "<b>Skip at most (minutes):</b><br>"
        "Time limit for skipping advances in Twinleaf Town.",
        LockMode::LOCK_WHILE_RUNNING,
        30, 5, 1440
    )
    , AUTO_CALIBRATE(
        "<b>Correct the timing automatically:</b><br>"
        "Shift the target advances when several attempts miss.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , USE_SOUND_DETECTION(
        "<b>Use sound detection:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , TAKE_VIDEO(
        "<b>Take Video:</b><br>"
        "Record a video when the shiny is found.",
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_SHINY(
        "Shiny Starter",
        true, true, ImageAttachmentMode::JPG,
        {"Notifs", "Showcase"}
    )
    , NOTIFICATIONS({
        &NOTIFICATION_SHINY,
        &NOTIFICATION_ERROR_RECOVERABLE,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    set_default_shiny_row(FILTERS);

    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(PLAYER_MODEL);
    PA_ADD_OPTION(START_POINT);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(FILTERS);
    PA_ADD_OPTION(COLLECTION_DISPLAY);
    PA_ADD_OPTION(STATE_DISPLAY);
    PA_ADD_OPTION(TARGET_DISPLAY);
    PA_ADD_OPTION(MAX_RESETS);
    PA_ADD_OPTION(MAX_TARGET_WAIT_MINUTES);
    PA_ADD_OPTION(MAX_SKIP_MINUTES);
    PA_ADD_OPTION(AUTO_CALIBRATE);
    PA_ADD_OPTION(USE_SOUND_DETECTION);
    PA_ADD_OPTION(TAKE_VIDEO);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);

    START_POINT.add_listener(*this);
    StarterRng::on_config_value_changed(this);
}


void StarterRng::on_config_value_changed(void*){
    MAX_SKIP_MINUTES.set_visibility(
        START_POINT == BdspStartPoint::Bedroom
            ? ConfigOptionState::ENABLED
            : ConfigOptionState::HIDDEN
    );
}


void StarterRng::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    StarterRng_Descriptor::Stats& stats = env.current_stats<StarterRng_Descriptor::Stats>();

    require_player(env.console, context, BUTTON_B);

    m_aim.reset();

    BdspRngFilterSnapshot filters = FILTERS.make_snapshot();
    StarterRngSettings settings{
        .language = LANGUAGE,
        .player_model = PLAYER_MODEL.model_number(),
        .start_point = START_POINT,
        .starter = STARTER,
        .wanted = [&filters](const BdspPokemonResult& result){
            return starter_wanted(filters, result);
        },
        .filters = &filters,
        .max_resets = MAX_RESETS,
        .max_target_wait_minutes = MAX_TARGET_WAIT_MINUTES,
        .max_skip_minutes = MAX_SKIP_MINUTES,
        .auto_calibrate = AUTO_CALIBRATE,
        .use_sound_detection = USE_SOUND_DETECTION,
        .take_video = TAKE_VIDEO,
        .reset_first = false,
        .finish_battle_on_shiny = false,
        .close_summary_after_read = false,
    };
    StarterRngAttemptIO io{
        .collection_display = &COLLECTION_DISPLAY,
        .state_display = &STATE_DISPLAY,
        .target_display = &TARGET_DISPLAY,
        .notification_shiny = &NOTIFICATION_SHINY,
        .notification_error = &NOTIFICATION_ERROR_RECOVERABLE,
        .stats = &stats,
    };

    run_starter_rng_attempts(env, context, settings, io, m_aim);

    GO_HOME_WHEN_DONE.run_end_of_program(context);
}


}
}
}
