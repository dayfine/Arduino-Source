/*  BDSP AutoStory - Segment 00.02: Lake Verity
 *  Into Lake Verity with Barry, open the briefcase, pick the starter and
 *  fight the Starly.
 *
 *  Start: outside Lake Verity, before picking the starter.
 *  End:   outside Lake Verity, after the starter battle.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/ImageMatchDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_StoryBattle.h"

#include "PokemonBDSP_AutoStory_Segment_00.h"

using namespace std::chrono_literals;
using PokemonAutomation::Pokemon::MapSet;
using PokemonAutomation::Pokemon::TilePos;
using PokemonAutomation::Pokemon::TrainerSight;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // Starter selection
            // ---------------------------------------------------------------------------

            static void select_turtwig(VideoStream& stream, ProControllerContext& context) {
                stream.log("[DEBUG] Turtwig Selection: Starting starter selection");

                pbf_press_button(context, BUTTON_ZL, 160ms, 240ms);
                context.wait_for_all_requests();

                ShortDialogWatcher watcher(COLOR_CYAN);
                int ret = run_until<ProControllerContext>(
                    stream, context,
                    [](ProControllerContext& ctx) { ctx.wait_for(std::chrono::milliseconds(5000)); },
                    { watcher }
                );
                if (ret != 0) {
                    ShortDialogWatcher watcher_red(COLOR_RED);
                    ret = run_until<ProControllerContext>(
                        stream, context,
                        [](ProControllerContext& ctx) { ctx.wait_for(std::chrono::milliseconds(5000)); },
                        { watcher_red }
                    );
                }
                stream.log(ret == 0 ? "[DEBUG] Turtwig Selection: Confirmation dialogue detected"
                    : "[WARNING] Turtwig Selection: Dialogue detection failed, proceeding anyway");

                pbf_wait(context, 400ms);
                pbf_press_dpad(context, DPAD_UP, 80ms, 400ms);
                pbf_press_button(context, BUTTON_ZL, 80ms, 5000ms);
                context.wait_for_all_requests();
                stream.log("[DEBUG] Turtwig Selection: Selection complete");
            }

            static void select_chimchar(VideoStream& stream, ProControllerContext& context) {
                stream.log("[DEBUG] Chimchar Selection: Starting starter selection");

                pbf_press_dpad(context, DPAD_RIGHT, 160ms, 840ms);
                pbf_press_button(context, BUTTON_ZL, 160ms, 240ms);
                context.wait_for_all_requests();

                ShortDialogWatcher watcher(COLOR_CYAN);
                int ret = run_until<ProControllerContext>(
                    stream, context,
                    [](ProControllerContext& ctx) { ctx.wait_for(std::chrono::milliseconds(5000)); },
                    { watcher }
                );
                if (ret != 0) {
                    ShortDialogWatcher watcher_red(COLOR_RED);
                    ret = run_until<ProControllerContext>(
                        stream, context,
                        [](ProControllerContext& ctx) { ctx.wait_for(std::chrono::milliseconds(5000)); },
                        { watcher_red }
                    );
                }
                stream.log(ret == 0 ? "[DEBUG] Chimchar Selection: Confirmation dialogue detected"
                    : "[WARNING] Chimchar Selection: Dialogue detection failed, proceeding anyway");

                pbf_wait(context, 400ms);
                pbf_press_dpad(context, DPAD_UP, 80ms, 400ms);
                pbf_press_button(context, BUTTON_ZL, 80ms, 5000ms);
                context.wait_for_all_requests();
                stream.log("[DEBUG] Chimchar Selection: Selection complete");
            }

            static void select_piplup(VideoStream& stream, ProControllerContext& context) {
                stream.log("[DEBUG] Piplup Selection: Starting starter selection");

                pbf_press_dpad(context, DPAD_RIGHT, 160ms, 840ms);
                pbf_press_dpad(context, DPAD_RIGHT, 160ms, 840ms);
                pbf_press_button(context, BUTTON_ZL, 160ms, 240ms);
                context.wait_for_all_requests();

                ShortDialogWatcher watcher(COLOR_CYAN);
                int ret = run_until<ProControllerContext>(
                    stream, context,
                    [](ProControllerContext& ctx) { ctx.wait_for(std::chrono::milliseconds(5000)); },
                    { watcher }
                );
                if (ret != 0) {
                    ShortDialogWatcher watcher_red(COLOR_RED);
                    ret = run_until<ProControllerContext>(
                        stream, context,
                        [](ProControllerContext& ctx) { ctx.wait_for(std::chrono::milliseconds(5000)); },
                        { watcher_red }
                    );
                }
                stream.log(ret == 0 ? "[DEBUG] Piplup Selection: Confirmation dialogue detected"
                    : "[WARNING] Piplup Selection: Dialogue detection failed, proceeding anyway");

                pbf_wait(context, 400ms);
                pbf_press_dpad(context, DPAD_UP, 80ms, 400ms);
                pbf_press_button(context, BUTTON_ZL, 80ms, 5000ms);
                context.wait_for_all_requests();
                stream.log("[DEBUG] Piplup Selection: Selection complete");
            }
        // ---------------------------------------------------------------------------
        //  Segment 00-02 begins
        //  Navigates into lake Verity to recieve starter
        // ---------------------------------------------------------------------------
        static void starter_nav(VideoStream& stream, ProControllerContext& context) {
            // re-sync control
            pbf_press_button(context, BUTTON_B, 20ms, 200ms);
            pbf_wait(context, 2000ms);
            context.wait_for_all_requests();

            // continue movement
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 300ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 300ms, 0ms);

            wait_for_dialogue(stream, context, "Phase 10 (Barry before the starter selection)");

            pbf_mash_button(context, BUTTON_A, 1000ms);

            pbf_wait(context, 500ms);
            std::shared_ptr<const ImageRGB32> briefcase =
                std::make_shared<const ImageRGB32>(
                    RESOURCE_PATH() + "PokemonBDSP/StarterBriefcase.png"
                );

            //  Mash B until we see the briefcase.
            ImageMatchWatcher detector(briefcase, { 0.5, 0.1, 0.5, 0.7 }, 100, true);

            int ret = run_until<ProControllerContext>(
                stream, context,
                [](ProControllerContext& context) {
                    pbf_mash_button(context, BUTTON_B, 120000ms);
                },
                { {detector} }
            );

            if (ret == 0) {
                stream.log("Detected briefcase!");
            }
            else {
                stream.log("Timed out waiting for briefcase.");
                throw OperationFailedExceptionWithScreenshot(
                    ErrorReportMode::SEND_ERROR_REPORT,
                    "Failed to detect briefcase in Segment 00.",
                    stream
                );
            }

            //  Wait for briefcase to fully open.
            stream.log("Mashing B for briefcase to fully open.");
            pbf_mash_button(context, BUTTON_B, 2000ms);
        }
        static void post_starly_clenaup(VideoStream& stream, ProControllerContext& context) {
            pbf_mash_button(context, BUTTON_B, 60000ms);
        }
        // ---------------------------------------------------------------------------
        // End of Segment 00 - 02
        // Standing in front lake varity after starter selection.
        // ---------------------------------------------------------------------------
        static void checkpoint_0002(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-01: Starter acquisition
                    starter_nav(env.console, context);
                    // Segment 00-00 starter selection
                    switch (options.starter_choice) {
                    case StarterChoice::TURTWIG:  select_turtwig(env.console, context);  break;
                    case StarterChoice::CHIMCHAR: select_chimchar(env.console, context); break;
                    case StarterChoice::PIPLUP:   select_piplup(env.console, context);   break;
                    }
                    fight_starly(env.console, context);
                    post_starly_clenaup(env.console, context);
                },
                true
            );
        }
        // ========================
        // Checkpoint 0002 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0002::name()       const { return "00.02 - Outside Verity before starter"; }
        std::string AutoStory_Checkpoint_0002::start_text() const { return AutoStory_Checkpoint_0001().end_text(); }
        std::string AutoStory_Checkpoint_0002::end_text()   const { return "Standing outside Lake Verity after picking starter."; }

        void AutoStory_Checkpoint_0002::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0002(env, context, options, stats);
        }


        }
    }
}
