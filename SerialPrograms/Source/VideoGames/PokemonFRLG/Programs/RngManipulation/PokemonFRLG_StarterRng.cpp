/*  Starter RNG
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Strings/StringTools.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_RngNavigation.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_SeedsDatabase.h"
#include "PokemonFRLG_StarterRngAttempts.h"
#include "PokemonFRLG_StarterRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


StarterRng_Descriptor::StarterRng_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:StarterRng",
        Pokemon::STRING_POKEMON + " FRLG", "Starter RNG",
        "Programs/PokemonFRLG/StarterRng.html",
        "Automatically calibrate timings to hit a specific RNG target for FRLG starters.",
        PanelQualityStanding::OFFICIAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> StarterRng_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

StarterRng::~StarterRng(){
    SEED.remove_listener(*this);
}

StarterRng::StarterRng()
    : m_calibration_displays(
        "<font size=4><b>Calibration Displays</b></font> — These will update automatically as the program runs"
    )
    , m_game_info(
        "<font size=4><b>Game Information</b></font>"
    )
    , GAME_VERSION(
        "<b>Game Version:</b>",
        {
            {GameVersion::firered, "firered", "FireRed"},
            {GameVersion::leafgreen, "leafgreen", "LeafGreen"}
        },
        LockMode::LOCK_WHILE_RUNNING,
        GameVersion::firered
    )
    , LANGUAGE(
        "<b>Game Language:</b>",
        {
            Language::English,
            Language::Japanese,
            Language::Spanish,
            Language::French,
            Language::German,
            Language::Italian,
        },
        LockMode::LOCK_WHILE_RUNNING,
        true
    )
    , SOUND(
        "<b>Sound:</b><br>"
        "Your in-game sound setting. This affects the possible seeds.",
        {
            {SoundSetting::Mono, "mono", "Mono"},
            {SoundSetting::Stereo, "stereo", "Stereo"}
        },
        LockMode::LOCK_WHILE_RUNNING,
        SoundSetting::Mono
    )
    , m_target_settings(
        "<font size=4><b>Target Settings</b></font> — Get these from an RNG search tool"
    )
    , STARTER(
        "<b>Starter Species:</b>",
        {
            {StarterSpecies::bulbasaur, "bulbasaur", "Bulbasaur"},
            {StarterSpecies::squirtle, "squirtle", "Squirtle"},
            {StarterSpecies::charmander, "charmander", "Charmander"},
        },
        LockMode::LOCK_WHILE_RUNNING,
        StarterSpecies::bulbasaur
    )    
    , SEED(
        false,
        "<b>Target Seed:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        "70FE", "70FE"
    )
    , ADVANCES(
        "<b>Advances:</b><br>"
        "The total number of RNG advances for your target.",
        LockMode::LOCK_WHILE_RUNNING,
        10000, 940, 1000000000 // default, min
    )
    , m_program_settings(
        "<font size=4><b>Program Settings</b></font>"
    )
    , SEED_RADIUS(
        "<b>Nearby Seed Radius:</b><br>"
        "The number of nearby seeds on each side of the target to search when identifying which seed was hit.",
        LockMode::LOCK_WHILE_RUNNING,
        5, 1 // default, min
    )
    , MAX_RESETS(
        "<b>Max Resets:</b>",
        LockMode::UNLOCK_WHILE_RUNNING,
        50, 0 // default, min
    )
    , IGNORE_WILD_SHINIES(
        "<b>Ignore wild shinies</b><br>"
        "Do not stop the program when a wild shiny is encountered.",
        LockMode::LOCK_WHILE_RUNNING, 
        false // default
    )
    , PROFILE(
        "<b>User Profile Position:</b><br>"
        "The position, from left to right, of the Switch profile with the FRLG save you'd like to use.<br>"
        "If this is set to 0, Switch 1 defaults to the last-used profile, while Switch 2 defaults to the first profile (position 1)",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 8 // default, min, max
    )
    , TAKE_VIDEO(
        "<b>Take Video:</b><br>"
        "Record a video when the shiny is found.", 
        LockMode::LOCK_WHILE_RUNNING, 
        true // default
    )
    , GO_HOME_WHEN_DONE(true)
    , NOTIFICATION_SHINY(
        "Shiny found",
        true, true, ImageAttachmentMode::JPG,
        {"Notifs", "Showcase"}
    )
    , NOTIFICATION_STATUS_UPDATE("Status Update", true, false, std::chrono::seconds(3600))
    , NOTIFICATIONS({
        &NOTIFICATION_SHINY,
        &NOTIFICATION_STATUS_UPDATE,
        &NOTIFICATION_PROGRAM_FINISH,
    })
{
    PA_ADD_OPTION(m_calibration_displays);
    PA_ADD_OPTION(RNG_TARGET);
    PA_ADD_OPTION(RNG_FILTERS);
    PA_ADD_OPTION(RNG_CALIBRATION);
    PA_ADD_OPTION(m_game_info);
    PA_ADD_OPTION(GAME_VERSION);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(SOUND);
    PA_ADD_OPTION(m_target_settings);
    PA_ADD_OPTION(STARTER);
    PA_ADD_OPTION(SEED);
    PA_ADD_OPTION(ADVANCES);
    PA_ADD_OPTION(m_program_settings);
    PA_ADD_OPTION(SEED_RADIUS);
    PA_ADD_OPTION(MAX_RESETS);
    PA_ADD_OPTION(PROFILE);
    PA_ADD_OPTION(TAKE_VIDEO);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);

    SEED.add_listener(*this);
}

void StarterRng::on_config_value_changed(void* object){
    if (object == &SEED){
        SEED.set(StringTools::strip(static_cast<std::string>(SEED)));
        return;
    }
}





void StarterRng::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    /*
    * Settings: Text Speed fast
    */

    StarterRngStats& stats = env.current_stats<StarterRng_Descriptor::Stats>();

    home_black_border_check(env.console, context);

    RNG_FILTERS.reset();
    RNG_CALIBRATION.reset_hits();

    const uint16_t TARGET_SEED = parse_seed(env.console, SEED);
    SeedsDatabase seeds_db(seeds_json_path(GAME_VERSION == GameVersion::firered, LANGUAGE));
    const SeedMatch seed_match = seeds_db.find_seed(TARGET_SEED, SOUND, SEED_RADIUS);
    if (!seed_match.found){
        throw UserSetupError(env.console, "The target Seed was not found in the seed database for this game version, language, and sound setting.");
    }
    env.log("Seed delay: " + std::to_string(seed_match.seed_delay) + "ms, button mode: " + seed_match.button_mode
        + ", column: " + seed_match.column_name);

    StarterRngAttempts attempts(
        StarterRngTarget{
            .starter = STARTER,
            .seed = TARGET_SEED,
            .advances = ADVANCES,
            .seed_match = seed_match,
        },
        StarterRngSettings{
            .language = LANGUAGE,
            .profile = PROFILE,
            .ignore_wild_shinies = IGNORE_WILD_SHINIES,
            .take_video = TAKE_VIDEO,
            .max_resets = [this]{ return uint64_t(MAX_RESETS); },
        },
        StarterRngDisplays{
            .target = RNG_TARGET,
            .filters = RNG_FILTERS,
            .calibration = RNG_CALIBRATION,
        },
        StarterRngNotifications{
            .shiny = NOTIFICATION_SHINY,
            .status_update = NOTIFICATION_STATUS_UPDATE,
            .error_recoverable = NOTIFICATION_ERROR_RECOVERABLE,
        },
        stats
    );
    StarterRngResult result = attempts.run(env, context);
    if (result.outcome == StarterRngOutcome::no_matches){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT,
            "Failed to find any matches 5 times in a row. Check your seed and advances settings.",
            env.console
        ); 
    }

    if (GO_HOME_WHEN_DONE){
        pbf_press_button(context, BUTTON_HOME, 200ms, 1000ms);
    }
    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);

}



}
}
}
