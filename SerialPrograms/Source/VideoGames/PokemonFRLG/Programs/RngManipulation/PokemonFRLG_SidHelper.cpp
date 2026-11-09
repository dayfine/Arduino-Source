/*  SID Helper
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <vector>
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ProgramStats/StatsTracking.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Pokemon_AdvRng.h"
#include "VideoGames/PokemonFRLG/Inference/Menus/PokemonFRLG_TrainerCardDetector.h"
#include "VideoGames/PokemonFRLG/Inference/PokemonFRLG_TrainerIdReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_StartMenuNavigation.h"
#include "VideoGames/PokemonFRLG/Programs/PokemonFRLG_TrainerCard.h"
#include "PokemonFRLG_SidSteps.h"
#include "PokemonFRLG_SidHelper.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

SidHelper_Descriptor::SidHelper_Descriptor()
    : SingleSwitchProgramDescriptor(
        "PokemonFRLG:SidHelper",
        Pokemon::STRING_POKEMON + " FRLG", "SID Helper",
        "Programs/PokemonFRLG/SidHelper.html",
        "Hit a specific RNG advance when starting a new game and determine the corresponding SID.",
        PanelQualityStanding::OFFICIAL,
        ProgramControllerClass::StandardController_RequiresPrecision,
        FeedbackType::REQUIRED,
        AllowCommandsWhenRunning::DISABLE_COMMANDS
    )
{}

struct SidHelper_Descriptor::Stats : public StatsTracker {
public:
    Stats()
        : errors(m_stats["Errors"])
    {
        m_display_order.emplace_back("Errors", HIDDEN_IF_ZERO);
    }

    std::atomic<uint64_t>& errors;
};

std::unique_ptr<StatsTracker> SidHelper_Descriptor::make_stats() const{
//    return std::unique_ptr<StatsTracker>(new Stats());
    return nullptr;
}

SidHelper::SidHelper()
    : LANGUAGE(
        "<b>Game Language:</b><br>"
        "Language affects the number of advances (based on the number of text characters) that pass after the last button press.",
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
    , TARGET_ADVANCES(
        "<b>Target Advances:</b><br>"
        "The target advances for finalizing the SID. This is arbitrary unless you're attempting to hit a specific TID/SID combination.<br>"
        "This value should always be odd.",
        LockMode::LOCK_WHILE_RUNNING, 
        3001, 2275 // default, min
    )
    , NUM_CANDIDATES(
        "<b># Candidate SIDs:</b><br>"
        "The number of SIDs to list near the target.",
        LockMode::LOCK_WHILE_RUNNING, 
        5, 1, 11 // default, min, max
    )
    , GO_HOME_WHEN_DONE(false)
    , NOTIFICATION_SIDS("SID Candidates", true, true)
    , NOTIFICATIONS({
        &NOTIFICATION_SIDS,
        &NOTIFICATION_PROGRAM_FINISH,
        &NOTIFICATION_ERROR_FATAL,
    })
{
    PA_ADD_OPTION(SIDS_DISPLAY);
    PA_ADD_OPTION(LANGUAGE);
    PA_ADD_OPTION(TARGET_ADVANCES);
    PA_ADD_OPTION(NUM_CANDIDATES);
    PA_ADD_OPTION(GO_HOME_WHEN_DONE);
    PA_ADD_OPTION(NOTIFICATIONS);
}

namespace{

std::vector<std::pair<std::string, std::string>> get_sid_messages(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    uint16_t tid,
    SimpleIntegerOption<uint32_t>& TARGET_ADVANCES,
    SimpleIntegerOption<uint8_t>& NUM_CANDIDATES
){
    std::vector<std::pair<std::string, std::string>> messages;

    int start = -1 * NUM_CANDIDATES / 2;
    int end = start + NUM_CANDIDATES;

    Pokemon::AdvRngSearcher searcher(tid, TARGET_ADVANCES + 2*start);

    for (int i = start; i < end; i++){
        std::pair<std::string, std::string> m;
        uint16_t sid = searcher.state.s0 >> 16;

        m.first = std::to_string(searcher.state.advance) + " Advances";
        m.second = std::to_string(sid);
        messages.push_back(m);

        env.log(m.first + ": " + m.second);

        searcher.advance_state();
        searcher.advance_state(); // 2 by 2
    }

    return messages;
}

} // namespace

void SidHelper::program(SingleSwitchProgramEnvironment& env, ProControllerContext& context){
    if (TARGET_ADVANCES % 2 == 0){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::NO_ERROR_REPORT, "SidHelper(): the Target Advances setting needs to be odd", env.console
        ); 
    }

    home_black_border_check(env.console, context);

    // SidHelper_Descriptor::Stats& stats = env.current_stats<SidHelper_Descriptor::Stats>();

    Language lang = LANGUAGE;
    const SidTiming timing = sid_timing(lang, TARGET_ADVANCES);
    env.log("Delay: " + std::to_string(timing.delay_ms) + "ms");

    set_sid_from_name_screen(context, timing);
    finish_intro_animations(env, context);
    navigate_to_trainer_card(env.console, context);

    uint16_t tid = read_trainer_card_tid(env.console, context, lang);

    std::vector<std::pair<std::string, std::string>> sid_messages = get_sid_messages(
        env, context, tid, TARGET_ADVANCES, NUM_CANDIDATES
    );

    SIDS_DISPLAY.set(tid, sid_messages);

    send_program_notification(env, NOTIFICATION_SIDS, COLOR_BLUE, "Possible SIDs:", sid_messages, "");

    send_program_finished_notification(env, NOTIFICATION_PROGRAM_FINISH);
    GO_HOME_WHEN_DONE.run_end_of_program(context);
}


}
}
}
