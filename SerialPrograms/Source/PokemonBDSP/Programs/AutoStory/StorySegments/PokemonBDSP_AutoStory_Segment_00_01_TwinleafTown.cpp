/*  BDSP AutoStory - Segment 00.00 and 00.01: Twinleaf Town
 *  00.00 checks the game's options before anything moves. 00.01 walks from
 *  the bedroom through Mom's and Barry's scenes to Route 201.
 *
 *  Start: in the player's bedroom, after the TV stops playing.
 *  End:   outside Lake Verity, before picking the starter.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "../Utils/PokemonBDSP_AutoStory_GameSettings.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"

#include "PokemonBDSP_AutoStory_Segment_00.h"
#include "PokemonBDSP_AutoStory_Segment_00_Shared.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

        // Navigation functions for each part of the tutorial. These functions are meant to be used
        // in order and rely on the state of the game being what it is at the end of the previous function.
        // They may also be modified or removed without warning.

        // ---------------------------------------------------------------------------
        // Segment 00-01 begins
        // Navigates from player's room to Route 201 through Mom/Barry dialogues.
        // ---------------------------------------------------------------------------

        static void tutorial_part_1_navigation(SingleSwitchProgramEnvironment& env, 
            VideoStream& stream, ProControllerContext& context) {
            DateTime t{ 2026, 1, 1, 0, 0, 0 };
            set_time_fast(env, context, t);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 527ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, 1.000000 }, 456ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 915ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, 1.000000 }, 642ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 557ms, 0ms);
            pbf_wait(context, 978ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1112ms, 0ms);
            pbf_wait(context, 4967ms);

            wait_for_dialogue(stream, context, "Phase 1 (Mom)");

            pbf_mash_button(context, BUTTON_A, 5000ms);
            pbf_wait(context, 300ms);

            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1662ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -1.000000 }, 49ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1215ms, 0ms);
            pbf_wait(context, 1419ms);

            wait_for_dialogue(stream, context, "Phase 2 (Mom Warning)");

            pbf_mash_button(context, BUTTON_A, 2500ms);
            pbf_wait(context, 300ms);
            context.wait_for_all_requests();

            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 680ms, 0ms);

            pbf_wait(context, 4000ms);

            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1748ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 18ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 2763ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 58ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1364ms, 0ms);
            pbf_wait(context, 4000ms);
            context.wait_for_all_requests();

            wait_for_dialogue(stream, context, "Phase 3 (Barry 1st)");
            pbf_mash_button(context, BUTTON_A, 6000ms);
            pbf_wait(context, 800ms);
            wait_for_dialogue(stream, context, "Phase 3 (Barry 1st)");
            pbf_mash_button(context, BUTTON_A, 6000ms);
            pbf_wait(context, 800ms);

            pbf_wait(context, 1306ms);
            context.wait_for_all_requests();
            stream.log("[Barry house] Entering; waiting for entry fade.");
            BlackScreenWatcher entry_black;
            int entry_ret = run_until<ProControllerContext>(stream, context,
                [](ProControllerContext& ctx) {
                    pbf_move_left_joystick(ctx, {0, 1}, 5000ms, 0ms);
                }, {{entry_black}});
            if (entry_ret != 0) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Barry house: Entry fade not detected.", stream);
            }
            // Release the joystick during loading; resume indoor movement only
            // after the screen has stayed non-black for half a second.
            BlackScreenWatcher entry_visible(COLOR_RED, {0.1, 0.1, 0.8, 0.8},
                100, 10, BlackScreenWatcher::FinderType::GONE, 500ms);
            if (wait_until(stream, context, 15s, {{entry_visible}}) != 0) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Barry house: Entry fade did not finish.", stream);
            }
            pbf_wait(context, 500ms);
            context.wait_for_all_requests();
            stream.log("[Barry house] Entry settled; starting stair approach.");
            // Align left after entry before the next upward stair approach.
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 400ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 0.000000 }, 1600ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 2722ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1600ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 501ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 652ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 1817ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 975ms, 0ms);
            pbf_wait(context, 1422ms);

            wait_for_dialogue(stream, context, "Phase 8 (Barry 2nd dialog)");
            pbf_mash_button(context, BUTTON_A, 10000ms);

            pbf_wait(context, 856ms);


            // Recorded route from the top of Barry's stairs. Moves rounded to
            // 100ms, waits to 50ms; transition waits gate both floor changes.
            recorded_move_until_transition(stream, context, -1.0, 0.0, 900ms,
                "Barry house downstairs");
            pbf_wait(context, 750ms);
            pbf_move_left_joystick(context, {-1.0, 0.0}, 2000ms, 0ms);
            pbf_wait(context, 250ms);
            pbf_move_left_joystick(context, {0.0, -1.0}, 2000ms, 0ms);
            pbf_wait(context, 150ms);
            pbf_move_left_joystick(context, {1.0, 0.0}, 1400ms, 0ms);
            pbf_wait(context, 500ms);
            recorded_move_until_transition(stream, context, 0.0, -1.0, 1200ms,
                "Barry house exit");
            // First-run video placed the player east of the northbound path.
            // Shorten the rightward approach; tune further from the next recording.
            stream.log("[Route 201] Aligning east for 2300ms before northbound approach.");
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 2300ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 5861ms, 0ms);
            pbf_wait(context, 134ms);

            wait_for_dialogue(stream, context, "Phase 9 (Barry Route 201)");

            pbf_mash_button(context, BUTTON_B, 6000ms);
            pbf_wait(context, 800ms);

            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 700ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 279ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 0.000000 }, 10ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 145ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 0.000000 }, 7703ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 140ms, 0ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 1700ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 39ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 0.000000 }, 544ms, 1000ms);

            pbf_wait(context, 1000ms);

            // save_game(stream, context);  // Disabled: checkpoint save mode controls saving.
            context.wait_for_all_requests();
        }
        // ---------------------------------------------------------------------------
        // End of Segment 00 - 01
        // Standing in front lake varity ready to enter lake Verity for starter selection.
        // ---------------------------------------------------------------------------
        std::string AutoStory_Checkpoint_0000::name() const { return "00.00 - Verify Settings"; }
        std::string AutoStory_Checkpoint_0000::start_text() const { return "Standing in the initial bedroom before beginning the tutorial."; }
        std::string AutoStory_Checkpoint_0000::end_text() const { return "Required game settings verified and saved; ready for 00.01."; }
        void AutoStory_Checkpoint_0000::run_checkpoint(
            SingleSwitchProgramEnvironment& env, ProControllerContext& context,
            AutoStoryOptions options, AutoStoryStats& stats
        ) const {
            // Before any route movement or checkpoint save: fail visibly if
            // options cannot be read, rather than navigate with unknown settings.
            verify_tutorial_settings(env.console, context);
            context.wait_for_all_requests();
        }

        static void checkpoint_0001(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-00: Intro and starter selection
                    tutorial_part_1_navigation(env, env.console, context);
                },
                true
            );
        }
        // ========================
        // Checkpoint 0001 Wrapper
        // ========================
        std::string AutoStory_Checkpoint_0001::name()       const { return "00.01 - After TV"; }
        std::string AutoStory_Checkpoint_0001::start_text() const { return "Game launched, Standing in the player's room after tv stops playing"; }
        std::string AutoStory_Checkpoint_0001::end_text()   const { return "Standing outside Lake Verity before picking starter."; }

        void AutoStory_Checkpoint_0001::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0001(env, context, options, stats);
        }


        }
    }
}
