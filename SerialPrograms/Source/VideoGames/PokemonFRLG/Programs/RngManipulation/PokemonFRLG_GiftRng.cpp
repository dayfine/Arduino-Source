/*  Gift RNG
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Common/Cpp/Strings/StringTools.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_RngNavigation.h"
#include "PokemonFRLG_HardReset.h"
#include "PokemonFRLG_RngCalibration.h"
#include "PokemonFRLG_RngLoopRoutines.h"
#include "PokemonFRLG_GiftRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


GiftRng_Descriptor::GiftRng_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:GiftRng",
        Pokemon::STRING_POKEMON + " FRLG", "Gift RNG",
        "Programs/PokemonFRLG/GiftRng.html",
        "Automatically calibrate timings to hit a specific RNG target for FRLG gift " + STRING_POKEMON,
        PanelQualityStanding::OFFICIAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

std::unique_ptr<StatsTracker> GiftRng_Descriptor::make_stats() const{
    return std::unique_ptr<StatsTracker>(new Stats());
}

GiftRng::~GiftRng(){
    SEED.remove_listener(*this);
}

GiftRng::GiftRng()
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
    , TARGET(
        "<b>Target:</b>",
        {
            {PokemonFRLG_RngTarget::magikarp, "magikarp", "Magikarp"},
            {PokemonFRLG_RngTarget::hitmonchan, "hitmonchan", "Hitmonchan"},
            {PokemonFRLG_RngTarget::hitmonlee, "hitmonlee", "Hitmonlee"},
            {PokemonFRLG_RngTarget::eevee, "eevee", "Eevee"},
            {PokemonFRLG_RngTarget::lapras, "lapras", "Lapras"},
            {PokemonFRLG_RngTarget::omanyte, "omanyte", "Omanyte"},
            {PokemonFRLG_RngTarget::kabuto, "kabuto", "Kabuto"},
            {PokemonFRLG_RngTarget::aerodactyl, "aerodactyl", "Aerodactyl"},
            {PokemonFRLG_RngTarget::gamecornerabra, "gamecornerabra", "Game Corner Abra"},
            {PokemonFRLG_RngTarget::gamecornerclefairy, "gamecornerclefairy", "Game Corner Clefairy"},
            {PokemonFRLG_RngTarget::gamecornerdratinifr, "gamecornerdratinifr", "Game Corner Dratini (FireRed)"},
            {PokemonFRLG_RngTarget::gamecornerdratinilg, "gamecornerdratinilg", "Game Corner Dratini (LeafGreen)"},
            {PokemonFRLG_RngTarget::gamecornerscyther, "gamecornerscyther", "Game Corner Scyther"},
            {PokemonFRLG_RngTarget::gamecornerpinsir, "gamecornerpinsir", "Game Corner Pinsir"},
            {PokemonFRLG_RngTarget::gamecornerporygon, "gamecornerporygon", "Game Corner Porygon"},
            {PokemonFRLG_RngTarget::togepi, "togepi", "Togepi"},
            {PokemonFRLG_RngTarget::togepifast, "togepifast", "Togepi (pre-approved)"}
        },
        LockMode::LOCK_WHILE_RUNNING,
        PokemonFRLG_RngTarget::magikarp
    )    
    , SEED(
        false,
        "<b>Target Seed:</b>",
        LockMode::LOCK_WHILE_RUNNING,
        "70FE", "70FE"
    )
    , ADVANCES(
        "<b>Advances:</b><br>The total number of RNG advances for your target.",
        LockMode::LOCK_WHILE_RUNNING,
        10000, 520, 1000000000 // default, min, max
    )
    , m_program_settings(
        "<font size=4><b>Program Settings</b></font>"
    )
    , USE_TEACHY_TV(
        "<b>Use Teachy TV:</b>"
        "<br>Opens the Teachy TV to quickly advance the RNG at 313x speed.<br>"
        "<i>Warning: can result in larger misses.</i>",
        LockMode::LOCK_WHILE_RUNNING,
        false // default
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
    , MAX_RARE_CANDIES(
        "<b>Max Rare Candies:</b><br>"
        "The number of rare candies in your bag. Make sure these are at the top position of the bag.<br>"
        "Rare candies used during calibration will be restored after resetting.",
        LockMode::UNLOCK_WHILE_RUNNING,
        0, 0, 95 // default, min, max
    )
    , PROFILE(
        "<b>User Profile Position:</b><br>"
        "The position, from left to right, of the Switch profile with the FRLG save you'd like to use.<br>"
        "If this is set to 0, Switch 1 defaults to the last-used profile, while Switch 2 defaults to the first profile (position 1)",
        LockMode::LOCK_WHILE_RUNNING,
        0, 0, 8 // default, min, max
    )
    , TAKE_VIDEO(
        "<b>Take Video:</b><br>Record a video when the shiny is found.", 
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
    PA_ADD_OPTION(TARGET);
    PA_ADD_OPTION(SEED);
    PA_ADD_OPTION(ADVANCES);
    PA_ADD_OPTION(m_program_settings);
    PA_ADD_OPTION(USE_TEACHY_TV);
    PA_ADD_OPTION(SEED_RADIUS);
    PA_ADD_OPTION(MAX_RESETS);
    PA_ADD_OPTION(MAX_RARE_CANDIES);
    PA_ADD_OPTION(PROFILE);
    PA_ADD_OPTION(TAKE_VIDEO);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);

    SEED.add_listener(*this);
}

void GiftRng::on_config_value_changed(void* object){
    if (object == &SEED){
        SEED.set(StringTools::strip(static_cast<std::string>(SEED)));
        return;
    }
}



void GiftRng::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    /*
    * Settings: Text Speed fast
    */

    GiftRngStats& stats = env.current_stats<GiftRng_Descriptor::Stats>();

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

    GiftRngAttempts attempts(
        GiftRngTarget{
            .target = TARGET,
            .seed = TARGET_SEED,
            .advances = ADVANCES,
            .seed_match = seed_match,
        },
        GiftRngSettings{
            .language = LANGUAGE,
            .profile = PROFILE,
            .use_teachy_tv = USE_TEACHY_TV,
            .take_video = TAKE_VIDEO,
            .identify_shiny = false,
            .max_resets = [this]{ return uint64_t(MAX_RESETS); },
            .max_rare_candies = [this]{ return uint64_t(MAX_RARE_CANDIES); },
            .on_reset = nullptr,
        },
        GiftRngDisplays{.target = RNG_TARGET, .filters = RNG_FILTERS, .calibration = RNG_CALIBRATION},
        GiftRngNotifications{
            .shiny = NOTIFICATION_SHINY,
            .status_update = NOTIFICATION_STATUS_UPDATE,
            .error_recoverable = NOTIFICATION_ERROR_RECOVERABLE,
        },
        stats
    );
    GiftRngResult result = attempts.run(env, context);
    if (result.outcome == GiftRngOutcome::no_matches){
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
