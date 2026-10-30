/*  BDSP AutoStory - Segment 00 Tools
 *
 *  Helper implementations for Segment 00.
 * 
 *  From: https://github.com/PokemonAutomation/
 *
 */


#include "PokemonBDSP/Inference/PokemonBDSP_TransitionWatchers.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include <algorithm>
#include <cmath>
#include <chrono>
#include <array>
#include <cctype>

#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "PokemonBDSP_Autostory_Segment_01_tools.h"

#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "Common/Cpp/PrettyPrint.h"
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "CommonFramework/Tools/ErrorDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"

#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "CommonTools/VisualDetectors/ImageMatchDetector.h"

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "NintendoSwitch/NintendoSwitch_Settings.h"

#include "NintendoSwitch/Programs/DateManip/NintendoSwitch_DateManip.h"
#include "NintendoSwitch/Programs/DateSpam/NintendoSwitch_HomeToDateTime.h"
#include "NintendoSwitch/Programs/NintendoSwitch_GameEntry.h"

#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"

#include "PokemonBDSP/Inference/Battles/PokemonBDSP_EndBattleDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_ReceivePokemonDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_ExperienceGainDetector.h"

#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"

#include "PokemonBDSP/Programs/PokemonBDSP_GameNavigation.h"
#include "PokemonBDSP/Programs/PokemonBDSP_BasicCatcher.h"
#include "PokemonBDSP/Programs/PokemonBDSP_GameEntry.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // Poketech Clown dialog helper
            // ---------------------------------------------------------------------------

            int detect_clown_choice_arrow(
                VideoOverlay& overlay,
                VideoSnapshot snapshot
            ) {
                SelectionArrowFinder top_arrow(
                    overlay,
                    { 0.62, 0.37, 0.08, 0.10 },
                    COLOR_RED
                );

                SelectionArrowFinder bottom_arrow(
                    overlay,
                    { 0.62, 0.48, 0.08, 0.10 },
                    COLOR_RED
                );

                if (top_arrow.detect(snapshot)) {
                    return 0;
                }

                if (bottom_arrow.detect(snapshot)) {
                    return 1;
                }

                return -1;
            }

            void clown_dialog_helper(
                VideoStream& stream,
                ProControllerContext& context,
                DpadPosition direction
            ) {
                stream.log(
                    "[AutoStory] Starting clown dialog helper...",
                    COLOR_BLUE
                );

                const auto overall_deadline =
                    std::chrono::steady_clock::now() + 30s;

                // -------------------------------------------------
                // Phase 1: Move + interact until dialog is detected.
                // -------------------------------------------------
                ShortDialogWatcher dialog(COLOR_YELLOW);
                bool dialog_started = false;

                while (!dialog_started) {

                    if (std::chrono::steady_clock::now() > overall_deadline) {
                        stream.log(
                            "[AutoStory] Clown helper timed out finding dialog.",
                            COLOR_RED
                        );
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "[AutoStory] Clown helper timed out finding dialog.", stream);
                    }

                    context.wait_for_all_requests();
                    pbf_press_dpad(context, direction, 80ms, 200ms);
                    pbf_press_button(context, BUTTON_A, 80ms, 500ms);

                    if (wait_until(stream, context, 1500ms, { {dialog} }) == 0) {
                        dialog_started = true;
                        stream.log("[AutoStory] Dialog detected.", COLOR_GREEN);
                    }
                }

                // -------------------------------------------------
                // Phase 2: A mash for 2 seconds.
                // -------------------------------------------------
                stream.log("[AutoStory] Mashing A to advance dialog...", COLOR_BLUE);
                pbf_mash_button(context, BUTTON_A, 4000ms);
                context.wait_for_all_requests();

                // -------------------------------------------------
                // Phase 3: Clear remaining dialog.
                // -------------------------------------------------
                pbf_mash_button(context, BUTTON_B, 4000ms);

                context.wait_for_all_requests();

                stream.log(
                    "[AutoStory] Clown dialog helper complete.",
                    COLOR_GREEN
                );
            }

            // ---------------------------------------------------------------------------
            // catch_1_pokemon_v2 - burst wiggle variant for testing.
            // Uses WIGGLES_PER_BURST consecutive left/right moves before checking for a
            // battle, reducing total wait time compared to checking after every move.
            // Commented out - enable for testing when ready.
            // ---------------------------------------------------------------------------
            // void catch_1_pokemon_v2(
            //     SingleSwitchProgramEnvironment& env,
            //     VideoStream& stream,
            //     ProControllerContext& context
            // ) {
            //     stream.log("[AutoStory] Starting catch_1_pokemon_v2 helper...", COLOR_BLUE);
            //
            //     // Save once before attempting to catch. On failure the game
            //     // resets back to this save, so the save lives outside the loop.
            //     stream.log("[AutoStory] Saving game before catch attempt...", COLOR_BLUE);
            //     {
            //         static constexpr int SAVE_MAX_RETRIES = 3;
            //         bool save_succeeded = false;
            //         for (int save_attempt = 0; save_attempt < SAVE_MAX_RETRIES; save_attempt++) {
            //             try {
            //                 if (save_attempt > 0) {
            //                     stream.log(
            //                         "[AutoStory] Retrying save (attempt " + std::to_string(save_attempt + 1) + ")...",
            //                         COLOR_ORANGE
            //                     );
            //                     pbf_mash_button(context, BUTTON_B, 1000ms);
            //                     context.wait_for_all_requests();
            //                 }
            //                 save_game(stream, context);
            //                 save_succeeded = true;
            //                 break;
            //             }
            //             catch (OperationFailedException& e) {
            //                 stream.log(
            //                     "[AutoStory] Save attempt " + std::to_string(save_attempt + 1) + " failed: " + e.message(),
            //                     COLOR_RED
            //                 );
            //                 context.wait_for(1000ms);
            //             }
            //         }
            //         if (!save_succeeded) {
            //             stream.log("[AutoStory] All save attempts failed. Using blind fallback save.", COLOR_ORANGE);
            //             pbf_mash_button(context, BUTTON_B, 1000ms);
            //             save_game(context);
            //             pbf_mash_button(context, BUTTON_B, 2000ms);
            //         }
            //     }
            //     context.wait_for_all_requests();
            //     pbf_wait(context, 1000ms);
            //
            //     // -------------------------------------------------------
            //     // Mini-checkpoint loop: retry from the save above if we
            //     // run out of balls or our Pokemon faints.
            //     // -------------------------------------------------------
            //     while (true) {
            //
            //     BlackScreenWatcher battle_start;
            //     BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
            //     BattleDialogWatcher battle_dialog(COLOR_YELLOW);
            //
            //     bool encounter_found = false;
            //
            //     // Wiggle left/right in bursts, checking for a battle after each burst.
            //     // Doing multiple moves before checking avoids waiting after every step.
            //     static constexpr int WIGGLES_PER_BURST = 3;
            //     while (!encounter_found) {
            //         for (int i = 0; i < WIGGLES_PER_BURST; i++) {
            //             pbf_move_left_joystick(context, {-1, 0}, 120ms, 80ms);
            //             pbf_move_left_joystick(context, { 1, 0}, 120ms, 80ms);
            //         }
            //         context.wait_for_all_requests();
            //
            //         int ret = wait_until(
            //             stream,
            //             context,
            //             1500ms,
            //             {
            //                 {battle_start},
            //                 {battle_menu},
            //                 {battle_dialog},
            //             }
            //         );
            //
            //         if (ret >= 0) {
            //             encounter_found = true;
            //         }
            //     }
            //
            //     stream.log("[AutoStory] Wild battle detected!", COLOR_GREEN);
            //
            //     // Wait until the actual battle menu appears.
            //     BattleMenuWatcher confirm_menu(BattleType::STANDARD);
            //
            //     int menu_ret = wait_until(
            //         stream,
            //         context,
            //         15s,
            //         {
            //             {confirm_menu},
            //         }
            //     );
            //
            //     if (menu_ret < 0) {
            //         stream.log(
            //             "[AutoStory] Failed to detect battle menu. Resetting to checkpoint...",
            //             COLOR_RED
            //         );
            //         reset_game_from_home(env, env.console, context, true);
            //         continue;
            //     }
            //
            //     stream.log("[AutoStory] Battle menu detected.", COLOR_GREEN);
            //
            //     context.wait_for_all_requests();
            //     pbf_wait(context, 500ms);
            //
            //     // --- Throw balls ---
            //     CatchResults result = basic_catcher(
            //         stream,
            //         context,
            //         Language::English,
            //         "poke-ball",
            //         10
            //     );
            //
            //     switch (result.result) {
            //     case CatchResult::POKEMON_CAUGHT:
            //         stream.log(
            //             "[AutoStory] Pokemon successfully caught after " +
            //             std::to_string(result.balls_used) + " ball(s).",
            //             COLOR_GREEN
            //         );
            //         {
            //             OverworldWatcher overworld;
            //             run_until<ProControllerContext>(
            //                 stream, context,
            //                 [](ProControllerContext& ctx) {
            //                     pbf_mash_button(ctx, BUTTON_B, 120000ms);
            //                 },
            //                 {{overworld}}
            //             );
            //         }
            //         return;
            //
            //     case CatchResult::POKEMON_FAINTED:
            //         stream.log("[AutoStory] Wild Pokemon fainted. Exiting battle.", COLOR_ORANGE);
            //         {
            //             OverworldWatcher overworld;
            //             run_until<ProControllerContext>(
            //                 stream, context,
            //                 [](ProControllerContext& ctx) {
            //                     pbf_mash_button(ctx, BUTTON_B, 120000ms);
            //                 },
            //                 {{overworld}}
            //             );
            //         }
            //         return;
            //
            //     case CatchResult::OUT_OF_BALLS:
            //         stream.log(
            //             "[AutoStory] Ran out of Poke Balls after " +
            //             std::to_string(result.balls_used) + " throw(s). Resetting to checkpoint...",
            //             COLOR_RED
            //         );
            //         reset_game_from_home(env, env.console, context, true);
            //         continue;
            //
            //     case CatchResult::OWN_FAINTED:
            //         stream.log(
            //             "[AutoStory] Own Pokemon fainted. Resetting to checkpoint...",
            //             COLOR_RED
            //         );
            //         reset_game_from_home(env, env.console, context, true);
            //         continue;
            //
            //     case CatchResult::BALL_LIMIT_REACHED:
            //         stream.log(
            //             "[AutoStory] Ball limit reached (" +
            //             std::to_string(result.balls_used) + " throw(s)). Resetting to checkpoint...",
            //             COLOR_RED
            //         );
            //         reset_game_from_home(env, env.console, context, true);
            //         continue;
            //
            //     case CatchResult::CANNOT_THROW_BALL:
            //         stream.log(
            //             "[AutoStory] Cannot throw ball (menu timing issue). Resetting to checkpoint...",
            //             COLOR_RED
            //         );
            //         reset_game_from_home(env, env.console, context, true);
            //         continue;
            //
            //     default:
            //         stream.log(
            //             "[AutoStory] Catch timed out after " +
            //             std::to_string(result.balls_used) + " ball(s). Resetting to checkpoint...",
            //             COLOR_RED
            //         );
            //         reset_game_from_home(env, env.console, context, true);
            //         continue;
            //     }
            //
            //     } // end while(true)
            // }

            // ---------------------------------------------------------------------------
            // Movement Helpers
            // ---------------------------------------------------------------------------

            bool detect_black_screen_after_movement(
                VideoStream& stream,
                ProControllerContext& context,
                Milliseconds timeout
            ) {
                BlackScreenWatcher black_screen;
                return wait_until(stream, context, timeout, {{black_screen}}) == 0;
            }

            bool try_right_down_exit_search(
                VideoStream& stream,
                ProControllerContext& context,
                size_t iterations
            ) {
                for (size_t i = 0; i < iterations; i++) {
                    pbf_move_left_joystick(context, { 1.0, 0.0 }, 80ms, 0ms);
                    pbf_move_left_joystick(context, { 0.0, -1.0 }, 220ms, 0ms);

                    if (detect_black_screen_after_movement(stream, context, 1000ms)) {
                        return true;
                    }
                }
                return false;
            }

            void leave_pokemon_center_before_barry_1(
                VideoStream& stream,
                ProControllerContext& context
            ) {
                // Original down movement to leave Pokemon Center.
                pbf_move_left_joystick(context, { 0.0, -1.0 }, 814ms, 0ms);

                if (detect_black_screen_after_movement(stream, context, 1200ms)) {
                    pbf_wait(context, 2340ms);
                    return;
                }

                stream.log("[AutoStory] Pokemon Center exit black screen not detected. Searching with right/down movement.", COLOR_ORANGE);

                if (try_right_down_exit_search(stream, context, 20)) {
                    pbf_wait(context, 2340ms);
                    return;
                }

                stream.log("[AutoStory] First exit search failed. Repositioning left and retrying.", COLOR_ORANGE);
                pbf_move_left_joystick(context, { -1.0, 0.0 }, 5000ms, 0ms);

                if (try_right_down_exit_search(stream, context, 30)) {
                    pbf_wait(context, 2340ms);
                    return;
                }

                OperationFailedExceptionWithScreenshot::fire(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Failed to find Pokemon Center exit black screen before Barry 1. Triggering checkpoint retry.",
                    stream
                );
            }

        }
    }
}
