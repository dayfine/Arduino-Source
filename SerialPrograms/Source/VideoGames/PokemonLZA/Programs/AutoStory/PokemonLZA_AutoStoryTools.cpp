/*  PokemonLZA AutoStory Tools
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/Notifications/ProgramNotifications.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_DialogDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_OverworldPartySelectionDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_GameEntry.h"
#include "PokemonLZA_AutoStoryTools.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;

void checkpoint_save(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    AutoStoryStats& stats
){
    bool ok = save_game_to_menu(env.console, context);
    if (!ok){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "checkpoint_save(): Failed to save game at checkpoint.",
            env.console
        );
    }
    stats.m_checkpoint++;
    env.update_stats();
    send_program_status_notification(env, notif_status_update, "Saved at checkpoint.");
}

namespace{

struct CheckpointContext{
    SingleSwitchProgramEnvironment& env;
    ProControllerContext& controller;
    EventNotificationOption& error_notif;
    AutoStoryStats& stats;
    const std::string& text;
};

void handle_checkpoint_failure(
    CheckpointContext& ctx,
    const OperationFailedException& e,
    size_t attempt,
    size_t max_attempts
){
    if (attempt == 10){
        auto snapshot = ctx.env.console.video().snapshot().frame;
        std::string message = "10 failed attempts: " + ctx.text;
        send_program_recoverable_error_notification_and_telemetry_report(
            ctx.env, &ctx.env.logger(), ctx.env.program_info(),
            ctx.error_notif,
            ErrorReportMode::SEND_ERROR_REPORT,
            message,
            "OperationFailedExceptionWithScreenshot",
            *snapshot,
            &ctx.env.console.history()
        );
    }

    if (attempt >= max_attempts){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Autostory checkpoint failed " + std::to_string(max_attempts) + " times.\n" +
            ctx.text + "\n" + e.message(),
            ctx.env.console
        );
    }

    ctx.controller.wait_for_all_requests();
    ctx.env.console.log("Resetting from checkpoint...");
    reset_game_from_home(ctx.env, ctx.env.console, ctx.controller);
    ctx.stats.m_reset++;
    ctx.env.update_stats();
}

} // namespace

void checkpoint_reattempt_loop(
    SingleSwitchProgramEnvironment& env,
    ProControllerContext& context,
    EventNotificationOption& notif_status_update,
    EventNotificationOption& notif_error_recoverable,
    AutoStoryStats& stats,
    const std::string& checkpoint_text,
    std::function<void(size_t attempt_number)>&& action
){
    const size_t max_attempts = 100;
    CheckpointContext ctx{env, context, notif_error_recoverable, stats, checkpoint_text};
    for (size_t i = 0;; i++){
        try{
            if (i == 0){
                checkpoint_save(env, context, notif_status_update, stats);
            }

            env.console.log(checkpoint_text + " Attempt: " + std::to_string(i));
            context.wait_for_all_requests();
            action(i);
            break;
        }catch (OperationFailedException& e){
            handle_checkpoint_failure(ctx, e, i, max_attempts);
        }
    }
}

void clear_dialog(
    ConsoleHandle& console,
    ProControllerContext& context,
    std::chrono::milliseconds timeout
){
    OverworldPartySelectionWatcher overworld(COLOR_WHITE, &console.overlay(), 200ms);
    FlatWhiteDialogWatcher flat_dialog(COLOR_RED, &console.overlay());
    BlueDialogWatcher blue_dialog(COLOR_BLUE, &console.overlay());
    TealDialogWatcher teal_dialog(COLOR_GREEN, &console.overlay());

    int ret = run_until<ProControllerContext>(
        console, context,
        [](ProControllerContext& context){
            pbf_mash_button(context, BUTTON_B, 60s);
        },
        {{overworld, flat_dialog, blue_dialog, teal_dialog}}
    );

    if (ret < 0){
        console.log("clear_dialog: timed out or nothing detected.", COLOR_RED);
    }
}

} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
