/*  AutoStory Checkpoint Hooks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_GameEntry.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_MenuNavigation.h"
#include "VideoGames/PokemonSV/Programs/PokemonSV_SaveGame.h"
#include "PokemonSV_AutoStoryTools.h"
#include "PokemonSV_AutoStoryCheckpointHooks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


SVCheckpointHooks::SVCheckpointHooks(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    EventNotificationOption& notif_error_recoverable,
    Pokemon::AutoStoryStats& stats,
    std::string fail_message,
    SVCheckpointMode mode
)
    : m_env(env), m_context(context)
    , m_notif_status_update(notif_status_update)
    , m_notif_error_recoverable(notif_error_recoverable)
    , m_stats(stats)
    , m_fail_message(std::move(fail_message))
    , m_mode(mode)
{}

void SVCheckpointHooks::log(const std::string& message){
    m_env.console.log(message);
}
void SVCheckpointHooks::wait_for_all_requests(){
    m_context.wait_for_all_requests();
}
void SVCheckpointHooks::save_checkpoint(){
    if (m_mode != SVCheckpointMode::TUTORIAL){
        checkpoint_save(m_env, m_context, m_notif_status_update, m_stats);
        return;
    }
    save_game_tutorial(m_env.program_info(), m_env.console, m_context);
    m_stats.m_checkpoint++;
    m_env.update_stats();
    send_program_status_notification(m_env, m_notif_status_update, "Saved at checkpoint.");
}
void SVCheckpointHooks::before_attempt(size_t attempt){
    if (m_mode == SVCheckpointMode::MAIN_STORY_DAY_SKIP && (attempt > 0 || ENABLE_TEST)){
        day_skip_from_overworld(m_env.console, m_context);
        save_game_from_overworld(m_env.program_info(), m_env.console, m_context);
    }
}
void SVCheckpointHooks::after_success(){
    if (m_mode != SVCheckpointMode::TUTORIAL){
        enter_menu_from_overworld(m_env.program_info(), m_env.console, m_context, -1);
    }
}
void SVCheckpointHooks::report_recoverable(const std::string& message){
    //  Send an error report for debugging.
    auto snapshot = m_env.console.video().snapshot().frame;
    send_program_recoverable_error_notification_and_telemetry_report(
        m_env, &m_env.logger(), m_env.program_info(),
        m_notif_error_recoverable,
        ErrorReportMode::SEND_ERROR_REPORT,
        message,
        "OperationFailedExceptionWithScreenshot",
        *snapshot,
        &m_env.console.history()
    );
}
void SVCheckpointHooks::fail(const OperationFailedException& e){
    OperationFailedExceptionWithScreenshot::fire(
        ErrorReportMode::SEND_ERROR_REPORT,
        m_fail_message + e.message(),
        m_env.console
    );
}
void SVCheckpointHooks::reset_for_retry(){
    reset_game(m_env.program_info(), m_env.console, m_context);
    m_stats.m_reset++;
    m_env.update_stats();
}


}
}
}
