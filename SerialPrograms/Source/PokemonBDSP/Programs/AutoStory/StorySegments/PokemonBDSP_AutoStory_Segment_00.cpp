/*  BDSP AutoStory - Segment 00
 *  From player's bedroom to pokedex acquisition.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/GlobalAutoPaths.h"
#include "PokemonBDSP_AutoStory_Segment_00.h"
#include "../Data/PokemonBDSP_AutoStory_JubilifeCity.h"
#include "../Data/PokemonBDSP_AutoStory_Route202.h"
#include "../Data/PokemonBDSP_AutoStory_SandgemTown.h"
#include "../Data/PokemonBDSP_AutoStory_StoryMaps.h"
#include "../Utils/PokemonBDSP_AutoStory_PokemonCenter.h"
#include "../Utils/PokemonBDSP_AutoStory_RouteWalk.h"
#include "../Utils/PokemonBDSP_AutoStory_Segment_00_tools.h"
#include "../Utils/PokemonBDSP_AutoStory_Walk.h"
#include "../Utils/PokemonBDSP_OverworldMatchers.h"

#include <algorithm>
#include <cmath>
#include <opencv2/imgproc.hpp>
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"

#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"

#include "Pokemon/Pokemon_Strings.h"
#include "Pokemon/Pokemon_Notification.h"

#include "PokemonBDSP/Programs/AutoStory/PokemonBDSP_AutoStoryTools.h"
#include "PokemonBDSP/Programs/ShinyHunting/PokemonBDSP_StarterReset.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "CommonTools/VisualDetectors/ImageMatchDetector.h"

#include "NintendoSwitch/Programs/DateManip/NintendoSwitch_DateManip.h"

#include "PokemonBDSP/Programs/PokemonBDSP_GameNavigation.h"

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

            // Watch before movement starts, release on the fade, and hold the
            // following recorded wait until the destination is visible again.
            static void recorded_move_until_transition(
                VideoStream& stream, ProControllerContext& context,
                double x, double y, Milliseconds duration, const std::string& label
            ) {
                context.wait_for_all_requests();
                stream.log("[Tutorial transition] " + label + ": approaching fade.");
                BlackScreenWatcher black;
                int ret = run_until<ProControllerContext>(stream, context,
                    [x, y, duration](ProControllerContext& ctx) {
                        pbf_move_left_joystick(ctx, {x, y}, duration, 0ms);
                        // Keep observing a delayed fade after the recorded hold.
                        pbf_wait(ctx, 5000ms);
                    }, {{black}});
                if (ret != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label + ": Black transition not detected.", stream);
                }
                BlackScreenWatcher visible(COLOR_RED, {0.1, 0.1, 0.8, 0.8},
                    100, 10, BlackScreenWatcher::FinderType::GONE, 500ms);
                if (wait_until(stream, context, 15s, {{visible}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label + ": Transition did not finish.", stream);
                }
                stream.log("[Tutorial transition] " + label + ": destination visible.");
            }

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
        // ---------------------------------------------------------------------------
        //  Segment 00-03 begins
        //  Navigates from Exit of lake Verity to receipt of the Pokedex
        // ---------------------------------------------------------------------------

        static void tutorial_part_2_navigation(SingleSwitchProgramEnvironment& env, 
            VideoStream& stream, ProControllerContext& context) {
            pbf_wait(context, 2000ms);
            context.wait_for_all_requests();

            pbf_wait(context, 563ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1700ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -1.000000 }, 510ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 50ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -1.000000 }, 310ms, 0ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 5500ms, 0ms);
            pbf_wait(context, 3963ms);
            context.wait_for_all_requests();

            wait_for_dialogue(stream, context, "Phase 10 (Professor dialog to mom)");
            pbf_mash_button(context, BUTTON_B, 40000ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 820ms, 0ms);
            pbf_wait(context, 4000ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 800ms, 0ms);
            pbf_wait(context, 555ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 50ms, 0ms);
            pbf_wait(context, 23ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 5020ms, 0ms);
            pbf_wait(context, 169ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 745ms, 0ms);
            DateTime t{ 2026, 1, 1, 0, 0, 50 };
            set_time_fast(env, context, t);
            walk_through_grass(stream, context, 1, 0, 20, "right through grass to path", BattleType::STARTER);
            walk_through_grass(stream, context, 0, 1, 18, "up through grass 2", BattleType::STARTER);
            walk_through_grass(stream, context, 1, 0, 10, "right through grass 2", BattleType::STARTER);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 730ms, 0ms);
            pbf_wait(context, 941ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 1950ms, 0ms);
            wait_for_dialogue(stream, context, "Phase 11 (dawn conversation)");
            pbf_mash_button(context, BUTTON_B, 50000ms);
            wait_for_dialogue(stream, context, "Phase 12 (Nickname reject)");
            pbf_mash_button(context, BUTTON_B, 40000ms);
            wait_for_dialogue(stream, context, "Phase 13 (Pokedex accept)");
            pbf_mash_button(context, BUTTON_A, 40000ms);


            // pokedex received
        }


        // ---------------------------------------------------------------------------
        // End of Segment 00 - 03
        // Standing in front of the professor after receiving the pokedex.
        // ---------------------------------------------------------------------------

        // ---------------------------------------------------------------------------
        // Begin Segment 00 - 04
        // Will heal at Sandgem pokeCenter to reliably catch a second pokemon for Home
        // transfer.
        // Collects poketech and stands in front of the PC to end the tutorial section
        // and send the starter to pokemon HOME.
        // ---------------------------------------------------------------------------

        // Inside the player's house, just through the door: Mom watches TV a
        // column east of the door and two rows north of the mat. The door fixes
        // our column, but counted taps north lost a row (r108b stood a row below
        // her). So find her row: face east and press A, and step one tile north
        // at a time until she answers. A press from a row off talks to nobody,
        // and the TV stand stops us a row above her.
        static void talk_to_mom_at_home(VideoStream& stream, ProControllerContext& context) {
            for (size_t row = 0; row < 4; row++) {
                if (talk_to_adjacent_npc(stream, context, {Pokemon::Direction::EAST}, "Home: Mom")) {
                    stream.log("[00.04] Mom answered after " + std::to_string(row) + " step(s) north.");
                    return;
                }
                // Facing east, a short tap north only turns; facing north, it is one tile.
                route_turn(stream, context, Pokemon::Direction::NORTH);
                pbf_press_dpad(context, DPAD_UP, 80ms, 400ms);
                context.wait_for_all_requests();
            }
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "Home: Mom did not answer from any of four rows north of the door.", stream);
        }

        static void tutorial_part_3_navigation(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context) {
            // Arm exit detection before movement and finish the fade before
            // starting the dialogue watcher, which cancels queued commands.
            recorded_move_until_transition(stream, context, 0.0, -1.0, 2010ms,
                "Rowan lab exit");
            context.wait_for_all_requests();

            wait_for_dialogue(stream, context, "Phase 14 (dawn tour)");

            pbf_mash_button(context, BUTTON_B, 25000ms);

            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 5125ms, 0ms);
            pbf_wait(context, 336ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1700ms, 0ms);
            pbf_wait(context, 213ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 2325ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, 1.000000 }, 950ms, 0ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 2360ms, 0ms);
            pbf_wait(context, 253ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 4610ms, 0ms);
            pbf_wait(context, 954ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 220ms, 0ms);
            pbf_wait(context, 587ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 1375ms, 0ms);
            pbf_wait(context, 464ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 775ms, 0ms);
            pbf_wait(context, 1655ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 640ms, 0ms);
            pbf_wait(context, 3128ms);
            talk_to_mom_at_home(stream, context);

            wait_for_dialogue(stream, context, "Phase 15 (Mom conversation to accept parcel)");

            pbf_mash_button(context, BUTTON_B, 40000ms);

            context.wait_for_all_requests();
            pbf_wait(context, 1500ms);
            pbf_press_dpad(context, DPAD_LEFT, 150ms, 0ms);
            pbf_wait(context, 1143ms);
            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 875ms, 0ms);
            pbf_wait(context, 1169ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 946ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 9092ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 740ms);
            pbf_press_button(context, BUTTON_A, 180ms, 0ms);
            pbf_wait(context, 2998ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 240ms, 0ms);
            pbf_wait(context, 893ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 5300ms, 0ms);
            pbf_wait(context, 282ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 260ms, 0ms);
            context.wait_for_all_requests();
            pbf_wait(context, 1500ms);

            DateTime t{ 2026, 1, 1, 0, 0, 50 };
            set_time_fast(env, context, t);
            walk_through_grass(stream, context, 1, 0, 20, "right through grass to path", BattleType::STARTER);
            walk_through_grass(stream, context, 0, 1, 18, "up through grass 2", BattleType::STARTER);
            walk_through_grass(stream, context, 1, 0, 10, "right through grass 2", BattleType::STARTER);
            context.wait_for_all_requests();
            pbf_wait(context, 1500ms);

            pbf_move_left_joystick(context, { 0.000000, -1.000000 }, 560ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 5000ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1000ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 1855ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 1.000000, -0.000000 }, 430ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { 0.000000, 1.000000 }, 850ms, 0ms);
            pbf_wait(context, 1500ms);
            pbf_move_left_joystick(context, { -1.000000, -0.000000 }, 1120ms, 0ms);

            wait_for_dialogue(stream, context, "Phase 16 (catch tutorial)");

            pbf_mash_button(context, BUTTON_B, 70000ms);
            context.wait_for_all_requests();
            pbf_wait(context, 2000ms);
            catch_1_pokemon(stream, context);
            pbf_wait(context, 1500ms);
            context.wait_for_all_requests();
        }

        // Align to a route reference before continuing recorded navigation.
        // Matcher offsets and thresholds are fractions of screen width/height.
        static void align_to_tutorial_landmarks(
            VideoStream& stream, ProControllerContext& context,
            OverworldMatchResult (*matcher)(const ImageRGB32&), const std::string& label
        ) {
            context.wait_for_all_requests();
            pbf_wait(context, 700ms);
            context.wait_for_all_requests();
            size_t confirmations = 0, coarse_confirmations = 0;
            double previous_dx = 0, previous_dy = 0;
            for (size_t attempt = 0; attempt < 60; attempt++) {
                const auto frame = stream.video().snapshot();
                if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    label + ": No video frame.", stream);
                const auto result = matcher(*frame.frame);
                VideoOverlaySet overlays(stream.overlay());
                for (const auto& landmark : result.landmarks) {
                    overlays.add(COLOR_CYAN, landmark.search);
                    if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                    stream.log("[" + label + "] " + landmark.name +
                        " score=" + std::to_string(landmark.score) +
                        " margin=" + std::to_string(landmark.margin) +
                        " accepted=" + std::to_string(landmark.accepted) +
                        " screen-relative offset=(" + std::to_string(landmark.dx) +
                        "," + std::to_string(landmark.dy) + ")");
                }
                stream.log("[" + label + "] located=" + std::to_string(result.located) +
                    " mode=" + (result.coarse ? "vertical acquisition" : "fine alignment") +
                    " reference screen-relative offset=(" + std::to_string(result.dx) +
                    "," + std::to_string(result.dy) + ")");
                if (result.coarse && result.located) {
                    if (coarse_confirmations && std::abs(result.dx-previous_dx)<=0.00625 &&
                        std::abs(result.dy-previous_dy)<=1.0/90) coarse_confirmations++;
                    else coarse_confirmations=1;
                    previous_dx=result.dx;
                    previous_dy=result.dy;
                } else coarse_confirmations=0;
                if (!result.located) {
                    confirmations = 0;
                    pbf_wait(context, 250ms);
                } else if (result.coarse && coarse_confirmations<3) {
                    confirmations = 0;
                    stream.log("[" + label + "] Waiting for consistent vertical acquisition.");
                    pbf_wait(context, 250ms);
                } else if (result.at_target) {
                    if (++confirmations >= 3) {
                        stream.log("[" + label + "] Reference position confirmed.");
                        return;
                    }
                    pbf_wait(context, 300ms);
                } else {
                    confirmations = 0;
                    const bool horizontal = !result.coarse &&
                        std::abs(result.dx) >= std::abs(result.dy)*9.0/16;
                    const double error = horizontal ? result.dx : result.dy*9.0/16;
                    const auto duration = std::abs(error) > 1.0/24 ? 80ms :
                        std::abs(error) > 0.015625 ? 40ms : 16ms;
                    stream.log("[" + label + "] Correcting " +
                        std::string(horizontal ? "horizontal" : "vertical") + " axis " +
                        (horizontal ? (result.dx>0 ? "right" : "left") : (result.dy<0 ? "up" : "down")) +
                        " for " + std::to_string(duration.count()) + "ms.");
                    // Reuse confirmed-Run encounter handling if correction enters grass.
                    walk_through_grass(stream, context,
                        horizontal ? (result.dx > 0 ? 1 : -1) : 0,
                        horizontal ? 0 : (result.dy < 0 ? 1 : -1),
                        1, label, BattleType::STANDARD, duration);
                }
                context.wait_for_all_requests();
            }
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": Reference position not confirmed; route movement stopped.", stream);
        }

        static void align_to_sandgem_junction(VideoStream& stream, ProControllerContext& context) {
            align_to_tutorial_landmarks(stream, context, match_sandgem_junction, "Sandgem junction");
        }

        // What the post-catch half of 00.04 does, logged up front and as each
        // step starts. A Lv5 Piplup fainted to Route 202's trainers (r72), so:
        // training, a heal at the Center, and Potions (they also saved the
        // Barry fight in r85) before Route 202's trainers. The Poke Mart lady
        // on Route 201 gives 10 for free, but she wanders between her grass
        // row and the open ground above it, and eight runs (r113a..r120a)
        // never got her talk; the Mart's clerk stays put.
        static const char* const AFTER_CATCH_STEPS[] = {
            "Train: fight 3 wild Pokemon in the catch grass, with the best move",
            "Walk south into Sandgem Town and heal at its Pokemon Center",
            "Buy 5 Potions at the Poke Mart next door, and check the Bag has them",
            "Walk back to Route 202's save spot and save",
            "Walk north up Route 202 to the Jubilife exit, fighting trainers who see us; after each, use a Potion if HP is under half",
            "Enter Jubilife and get through Dawn's scene",
        };
        constexpr size_t AFTER_CATCH_STEP_COUNT = sizeof(AFTER_CATCH_STEPS) / sizeof(AFTER_CATCH_STEPS[0]);

        static void log_after_catch_steps(VideoStream& stream) {
            std::string text = "[00.04] After the catch, the steps are:";
            for (size_t i = 0; i < AFTER_CATCH_STEP_COUNT; i++) {
                text += "\n    " + std::to_string(i + 1) + ". " + AFTER_CATCH_STEPS[i];
            }
            stream.log(text, COLOR_BLUE);
        }

        static void after_catch_step(VideoStream& stream, size_t number) {
            const std::string count = std::to_string(number) + "/" + std::to_string(AFTER_CATCH_STEP_COUNT);
            std::string text = "[00.04 step " + count + "] " + AFTER_CATCH_STEPS[number - 1];
            text += number < AFTER_CATCH_STEP_COUNT ? std::string(" -- next: ") + AFTER_CATCH_STEPS[number] : std::string(" -- last step");
            stream.log(text, COLOR_BLUE);
            add_wrapped_log(stream.overlay(), "00.04 step " + count + ": " + AFTER_CATCH_STEPS[number - 1], COLOR_WHITE);
            if (number < AFTER_CATCH_STEP_COUNT) {
                add_wrapped_log(stream.overlay(), "Next: " + std::string(AFTER_CATCH_STEPS[number]), COLOR_GRAY);
            }
        }

        // Fights a trainer whose dialogue has opened, with whatever starter we
        // have. Someone who only talks leaves no battle to fight.
        static void fight_route_trainer(VideoStream& stream, ProControllerContext& context, const std::string& name) {
            pbf_mash_button(context, BUTTON_B, 5000ms);
            context.wait_for_all_requests();
            BattleMenuWatcher battle_menu(BattleType::TRAINER, COLOR_YELLOW);
            BattleDialogWatcher battle_dialog(COLOR_YELLOW);
            if (wait_until(stream, context, 8s, {{battle_menu}, {battle_dialog}}) < 0) {
                stream.log("[00.04] " + name + ": no battle followed the talk.");
                pbf_mash_button(context, BUTTON_B, 2000ms);
                context.wait_for_all_requests();
                return;
            }
            fight_trainer(stream, context, name);
            context.wait_for_all_requests();
            // The next trainer may be a few tiles on: check HP now. With no
            // Potion in the Bag this only looks.
            use_potion_first_pokemon(stream, context, 0.5);
        }

        // The starter starts around level 5 with only a Normal move; the Route
        // 202 trainers need a level or two more. Starts from either save: the
        // catch spot (00.03's save, in the grass) or the save spot under the
        // ledge.
        static void train_in_catch_grass(
            VideoStream& stream, ProControllerContext& context, const MapSet& maps,
            std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
        ) {
            const std::vector<TilePos> grass = story_after_training_area();
            walk_route(stream, context, maps, story_save_spots(),
                [&](const TilePos& pos) { return std::find(grass.begin(), grass.end(), pos) != grass.end(); },
                trainers, fight, "To the catch grass");
            for (size_t battle = 0; battle < 3; battle++) {
                route_status(stream, "Training battle " + std::to_string(battle + 1) + "/3");
                battle_1_pokemon(stream, context);
            }
        }

        // South into Sandgem Town, to the tile below the Pokemon Center's sign,
        // which the house west of its column and the sign pin (r75 walked this
        // and healed). The Center's door is three tiles west; the heal lines up
        // with it by sight. Returns where we stand after: below the door.
        static std::vector<TilePos> heal_at_sandgem_center(
            VideoStream& stream, ProControllerContext& context, const MapSet& maps,
            std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
        ) {
            const TilePos below_sign = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_SIGN.next(Pokemon::Direction::SOUTH));
            walk_route(stream, context, maps, story_after_training_area(),
                [&](const TilePos& pos) { return pos == below_sign; }, trainers, fight, "To the Pokemon Center");
            heal_at_pokemon_center(stream, context, "Sandgem Town Pokemon Center");
            return story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR);
        }

        // How many Potions to buy. A Potion is 300 and a new game starts with
        // 3000; the money on screen cannot be read yet. Five heal the starter
        // through Route 202's four trainers and leave half the money.
        constexpr int POTIONS_TO_BUY = 5;

        // From below the Center's door to below the Poke Mart's, ten tiles
        // east along the same row, and in. The last leg is open and movement
        // is not locked to tiles, so we may stand part of a tile off the door:
        // whole-tile steps either side missed it in r121a. The Mart's doorway
        // is lit like the Center's, so it is lined up with by sight the same
        // way. Two townspeople wander the dirt south of the Mart and stop a
        // leg short without the walk knowing (r122a counted to the door's
        // row from three rows south of it): where the doorway shows on screen
        // says so, and the walk is made once more from there.
        static void enter_sandgem_poke_mart(
            VideoStream& stream, ProControllerContext& context, const MapSet& maps,
            std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
        ) {
            const TilePos door = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR);
            const TilePos front = door.next(Pokemon::Direction::SOUTH);
            const auto at_front = [&](const TilePos& pos) { return pos.y == front.y && std::abs(pos.x - front.x) <= 1; };
            walk_route(stream, context, maps, story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKEMON_CENTER_DOOR),
                at_front, trainers, fight, "To the Poke Mart");
            const std::vector<TilePos> short_of_door = tiles_short_of_door(stream, context, maps.world(), door, "To the Poke Mart");
            if (!short_of_door.empty()) {
                route_status(stream, "To the Poke Mart: stopped short of the door; walking again.");
                walk_route(stream, context, maps, short_of_door, at_front, trainers, fight, "To the Poke Mart again");
            }
            enter_lit_door(stream, context, "Sandgem Poke Mart: through the door");
        }

        // Inside the Mart, the clerk's counter is straight ahead of the door
        // (every Mart is laid out so; Oreburgh's and Eterna's are walked the
        // same way). Walk up to it, buy the Potions, and walk back out.
        // buy_items() opens the talk and finds the Potion row by OCR of its
        // name and price; the quantity is counted out blind.
        static void buy_potions_at_the_counter(VideoStream& stream, ProControllerContext& context) {
            route_status(stream, "Poke Mart: to the clerk.");
            const WalkResult walk = walk_until(stream, context, 0, 1, 6000ms, "Poke Mart: to the clerk");
            if (walk.stop != WalkStop::BLOCKED && walk.stop != WalkStop::DIALOG) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Poke Mart: the counter was not reached (" + std::string(walk_stop_name(walk.stop)) + ").", stream);
            }
            route_status(stream, "Poke Mart: buying " + std::to_string(POTIONS_TO_BUY) + " Potions.");
            if (!buy_items(stream, context, {{ShopItem::Potion, POTIONS_TO_BUY}})) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Poke Mart: could not buy " + std::to_string(POTIONS_TO_BUY) + " Potions.", stream);
            }
            leave_door_south(stream, context, "Poke Mart: out the door");
        }

        // Potions let us heal where we stand. Sandgem's Poke Mart is next
        // door east of its Center. The purchase is checked in the Bag after,
        // since buy_items() counts the quantity out without reading it.
        // Returns where we stand after: below the Mart's door.
        static std::vector<TilePos> buy_potions_in_sandgem(
            VideoStream& stream, ProControllerContext& context, const MapSet& maps,
            std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
        ) {
            enter_sandgem_poke_mart(stream, context, maps, trainers, fight);
            buy_potions_at_the_counter(stream, context);
            if (!bag_has_item(stream, context, "potion", 1)) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Poke Mart: the purchase went through, but no Potion is in the Bag.", stream);
            }
            return story_door_front(MAP_SANDGEM_TOWN, SandgemTown::POKE_MART_DOOR);
        }

        // From the trees at the east end of the doors' row, west into the gap
        // between the Center's sign and the Mart's (below_sign_gap()) and
        // north through it until a wall stops us. Six tiles west from column
        // 29 ends on 23, and on 20 to 26 if two tiles off: north from all of
        // those passes no door, and only the gap lets us through. A walk
        // north that goes nowhere stood below a sign or the Mart: a tile
        // west, twice at most, never reaches the Center's door at 17. The
        // planner would rather go round the south of town on walls, where
        // townspeople wander. Returns where the walk north may have ended.
        static std::vector<TilePos> north_through_sign_gap(VideoStream& stream, ProControllerContext& context, const MapSet& maps){
            using Pokemon::Direction;
            route_status(stream, "To the gap between the Center's and the Mart's signs: west 6.");
            route_turn(stream, context, Direction::WEST);
            walk_for(stream, context, -1, 0, route_open_hold(Direction::WEST, 6), "To the gap between the signs");
            for (size_t nudges = 0;; nudges++){
                const WalkResult north = walk_until(stream, context, 0, 1, 8000ms, "North through the gap");
                if (north.stop == WalkStop::TRANSITION || north.stop == WalkStop::DIALOG || north.wild_battles > 0){
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "North through the gap: " + std::string(walk_stop_name(north.stop)) + " instead of a wall.", stream);
                }
                //  Blocked at once: the wall reads after about 800ms still.
                if (north.walked > ROUTE_START_DELAY + 800ms + 2 * ROUTE_MS_PER_TILE_UP_DOWN){
                    break;
                }
                if (nudges == 2){
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "North through the gap: still blocked after two tiles west.", stream);
                }
                route_status(stream, "North through the gap: blocked at once, below a sign or the Mart; a tile west.");
                route_turn(stream, context, Direction::WEST);
                walk_for(stream, context, -1, 0, route_open_hold(Direction::WEST, 1), "A tile west");
                route_turn(stream, context, Direction::NORTH);
            }
            std::vector<TilePos> ends;
            for (const TilePos& pos : SandgemTown::below_sign_gap()){
                ends.push_back(Pokemon::walk_to_wall(maps.world(), maps.to_world(MAP_SANDGEM_TOWN, pos), Direction::NORTH).stop);
            }
            return ends;
        }

        // Back up to Route 202's save spot under the ledge and save there: it
        // is where a retry may start (story_save_spots()), its walls pin it,
        // and the trainers are north of it. A save in Sandgem would need the
        // training walk to plan from town as well. First east to the trees
        // that end the doors' row, so the way north starts from one known
        // tile, then north through the gap between the signs: r123a, planned
        // round the south of town on a map that had it wrong, walked into a
        // house.
        static void save_at_route_202_save_spot(
            SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context, const MapSet& maps,
            std::vector<TilePos> at, std::vector<TrainerSight>& trainers, const RouteTrainerFight& fight
        ) {
            const TilePos row_end = maps.to_world(MAP_SANDGEM_TOWN, SandgemTown::FRONT_ROW_EAST_END);
            const TilePos save_spot = maps.to_world(MAP_ROUTE_202, Route202::SAVE_SPOT);
            at = walk_route(stream, context, maps, std::move(at),
                [&](const TilePos& pos) { return pos == row_end; }, trainers, fight, "To the trees east of the Poke Mart");
            at = north_through_sign_gap(stream, context, maps);
            walk_route(stream, context, maps, std::move(at),
                [&](const TilePos& pos) { return pos == save_spot; }, trainers, fight, "Back to Route 202");
            stream.log("[00.04] Saving with the Potions, at the save spot on Route 202.");
            save_game(env, context);
        }

        static void meet_dawn_in_jubilife(VideoStream& stream, ProControllerContext& context) {
            // Jubilife's south edge is a wall with stairs in it, west of where the
            // route may leave us (r78 stood at its east end). Entering starts a
            // scene: Dawn walks over while we stand still. So walk north; if the
            // wall stops us and no dialogue follows, the stairs are further west.
            bool met_dawn = false;
            for (size_t attempt = 0; attempt < 6 && !met_dawn; attempt++) {
                const WalkResult walk = walk_until(stream, context, 0, 1, 4000ms, "to Dawn in Jubilife");
                if (walk.stop == WalkStop::TRANSITION) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Jubilife: left the area instead of meeting Dawn.", stream);
                }
                ShortDialogWatcher dialog(COLOR_CYAN);
                if (walk.stop == WalkStop::DIALOG || wait_until(stream, context, 6s, {{dialog}}) == 0) {
                    met_dawn = true;
                    break;
                }
                stream.log("[00.04] Jubilife: the wall stopped us; looking for the stairs one tile west.");
                route_turn(stream, context, Pokemon::Direction::WEST);
                pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Pokemon::Direction::WEST, 1), 200ms);
                context.wait_for_all_requests();
            }
            if (!met_dawn) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Jubilife: no stairs found and no Dawn after six tiles west.", stream);
            }
            wait_for_dialogue(stream, context, "Phase 21 (dawn Jubilife dialog)");

            pbf_mash_button(context, BUTTON_B, 5000ms);
        }

        // Resumes from the save made after catching the second Pokemon, or
        // from step 4's save at Route 202's save spot. The catch save has no
        // Potions; step 4's has them, with the starter trained and healed, so
        // Potions in the Bag mean steps 1 to 4 are done.
        static void tutorial_part_3_after_catch_navigation(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context) {
            // A retry reloads the save at whatever time the clock has reached.
            // Pin it again so the landmark references see the same lighting.
            set_time_fast(env, context, DateTime{ 2026, 1, 1, 0, 0, 50 });
            log_after_catch_steps(stream);

            const MapSet& maps = story_maps();
            std::vector<TrainerSight> trainers = story_trainer_sights();
            const RouteTrainerFight fight = [&](const std::string& name) { fight_route_trainer(stream, context, name); };
            pbf_wait(context, 1500ms);

            if (bag_has_item(stream, context, "potion", 1)) {
                stream.log("[00.04] The Bag has Potions: this is step 4's save; on to Route 202's trainers.", COLOR_BLUE);
            } else {
                after_catch_step(stream, 1);
                train_in_catch_grass(stream, context, maps, trainers, fight);

                after_catch_step(stream, 2);
                heal_at_sandgem_center(stream, context, maps, trainers, fight);

                after_catch_step(stream, 3);
                const std::vector<TilePos> at = buy_potions_in_sandgem(stream, context, maps, trainers, fight);

                after_catch_step(stream, 4);
                save_at_route_202_save_spot(env, stream, context, maps, at, trainers, fight);
            }

            after_catch_step(stream, 5);
            walk_route(stream, context, maps, story_save_spots(), story_at_jubilife_exit, trainers, fight, "To Jubilife");

            after_catch_step(stream, 6);
            meet_dawn_in_jubilife(stream, context);
        }
        // ---------------------------------------------------------------------------
        // End of Segment 00 - 04
        // Standing in Jubilife after speaking to dawn.
        // ---------------------------------------------------------------------------
        // ---------------------------------------------------------------------------
        // Begin Segment 00 - 05
        // Standing at the entrance to jubilife after speaking to dawn, will navigate to the poketech challenge.
        // Collects poketech and stands in front of the PC to end the tutorial section
        // and send the starter to pokemon HOME.
        // ---------------------------------------------------------------------------

            // Stop the approach as soon as buildings can guide it, instead of
            // overshooting south and then blindly walking back north.
            static void approach_poketch_landmarks(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                Milliseconds walked = 0ms;
                while (walked < 1900ms) {
                    auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Poketch approach: No video frame.", stream);
                    const auto result = match_poketch_landmarks(*frame.frame);
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[Poketch approach] " + landmark.name + " score=" + std::to_string(landmark.score) +
                            " margin=" + std::to_string(landmark.margin));
                    }
                    if (result.located) {
                        stream.log("[Poketch approach] Landmarks acquired after " + std::to_string(walked.count()) + "ms; ending downward approach.");
                        return;
                    }
                    const Milliseconds step = std::min(200ms, 1900ms - walked);
                    pbf_move_left_joystick(context, {0, -1}, step, 0ms);
                    walked += step;
                    pbf_wait(context, 700ms);
                    context.wait_for_all_requests();
                }
                stream.log("[Poketch approach] Reached 1900ms downward limit; handing off to locator.");
            }

            static void align_and_talk_to_poketch_man(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                pbf_wait(context, 700ms);
                context.wait_for_all_requests();
                size_t confirmed = 0, coarse_confirmed = 0;
                double previous_dx = 0, previous_dy = 0;
                size_t acquisition_misses = 0, vertical_probe = 0;
                // Two small excursions around the approach position. This is
                // visibility recovery, not permission to interact without a match.
                const int vertical_probes[] = {-80, 160, -240, 320, -160, -120, 240, -360, 480, -240};
                for (size_t attempt = 0; attempt < 80; attempt++) {
                    auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Poketch interaction: No video frame.", stream);
                    const OverworldMatchResult result = match_poketch_landmarks(*frame.frame);
                    stream.log(result.coarse ? "[Poketch landmarks] Coarse acquisition." : "[Poketch landmarks] Fine alignment.");
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[Poketch landmarks] " + landmark.name + " score=" + std::to_string(landmark.score) +
                            " margin=" + std::to_string(landmark.margin) + " accepted=" + std::to_string(landmark.accepted) +
                            " screen-relative offset=(" + std::to_string(landmark.dx) + "," + std::to_string(landmark.dy) + ")");
                    }
                    if (result.located) acquisition_misses = 0;
                    if (result.coarse && result.located) {
                        if (coarse_confirmed && std::abs(result.dx - previous_dx) <= 0.00625 && std::abs(result.dy - previous_dy) <= 1.0/90)
                            coarse_confirmed++;
                        else coarse_confirmed = 1;
                        previous_dx = result.dx; previous_dy = result.dy;
                    } else coarse_confirmed = 0;
                    if (!result.located) {
                        confirmed = 0;
                        if (++acquisition_misses >= 4 && vertical_probe < sizeof(vertical_probes) / sizeof(vertical_probes[0])) {
                            int probe = vertical_probes[vertical_probe++];
                            acquisition_misses = 0;
                            stream.log("[Poketch landmarks] Bounded vertical visibility probe " + std::to_string(vertical_probe) +
                                "/10: " + (probe < 0 ? "down " : "up ") + std::to_string(std::abs(probe)) + "ms.");
                            pbf_move_left_joystick(context, {0, probe < 0 ? -1.0 : 1.0}, Milliseconds(std::abs(probe)), 0ms);
                            pbf_wait(context, 700ms);
                        } else {
                            stream.log("[Poketch landmarks] No trustworthy acquisition; waiting for another frame.");
                            pbf_wait(context, 250ms);
                        }
                    } else if (result.coarse && coarse_confirmed < 3) {
                        confirmed = 0;
                        stream.log("[Poketch landmarks] Waiting for consistent coarse offset before movement.");
                        pbf_wait(context, 250ms);
                    } else if (result.at_target) {
                        if (++confirmed >= 3) {
                            stream.log("[Poketch landmarks] Above-NPC reference confirmed; turning down.");
                            pbf_press_dpad(context, DPAD_DOWN, 40ms, 400ms);
                            context.wait_for_all_requests();
                            pbf_press_button(context, BUTTON_A, 80ms, 500ms);
                            context.wait_for_all_requests();
                            wait_for_dialogue(stream, context, "Phase 27 (poketech challenge completed dialog)", 10s);
                            return;
                        }
                        pbf_wait(context, 300ms);
                    } else {
                        confirmed = 0;
                        const double dx = result.dx, dy = result.dy;
                        stream.log("[Poketch landmarks] Reference screen-relative offset=(" + std::to_string(result.dx) + "," + std::to_string(result.dy) + ").");
                        const bool horizontal = std::abs(dx) > 7.0/960 &&
                            (std::abs(dy) <= 7.0/540 || std::abs(dx) >= std::abs(dy)*9.0/16);
                        double error = horizontal ? dx : dy*9.0/16;
                        auto duration = std::abs(error) > 0.03125 ? 80ms : std::abs(error) > 0.015625 ? 40ms : 16ms;
                        if (std::abs(dx) <= 7.0/960 && std::abs(dy) <= 7.0/540) {
                            stream.log("[Poketch landmarks] Coarse position near target; waiting for fine confirmation.");
                            pbf_wait(context, 300ms);
                            context.wait_for_all_requests();
                            continue;
                        }
                        stream.log("[Poketch landmarks] Correcting " + std::string(horizontal ? "horizontal" : "vertical") +
                            " axis for " + std::to_string(duration.count()) + "ms.");
                        pbf_move_left_joystick(context,
                            {horizontal ? (dx > 0 ? 1.0 : -1.0) : 0.0,
                             horizontal ? 0.0 : (dy < 0 ? 1.0 : -1.0)}, duration, 0ms);
                        pbf_wait(context, 700ms);
                    }
                    context.wait_for_all_requests();
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Poketch interaction: Building landmarks did not confirm the reference position; A not pressed.", stream);
            }

            void align_and_enter_jubilife_center(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                pbf_wait(context, 700ms);
                context.wait_for_all_requests();
                size_t confirmed = 0;
                for (size_t attempt = 0; attempt < 60; attempt++) {
                    const auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Jubilife Center alignment: No video frame.", stream);
                    const auto result = match_jubilife_center_entrance(*frame.frame);
                    VideoOverlaySet overlays(stream.overlay());
                    for (const auto& landmark : result.landmarks) {
                        overlays.add(COLOR_CYAN, landmark.search);
                        if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                        stream.log("[Jubilife Center alignment] score=" + std::to_string(landmark.score) +
                            " margin=" + std::to_string(landmark.margin) + " screen-relative offset=(" +
                            std::to_string(landmark.dx) + "," + std::to_string(landmark.dy) + ")");
                    }
                    if (!result.located) {
                        confirmed = 0;
                        pbf_wait(context, 250ms);
                    } else if (result.at_target) {
                        if (++confirmed >= 3) {
                            stream.log("[Jubilife Center alignment] Door position confirmed; entering.");
                            recorded_move_until_transition(stream, context, 0, 1, 2000ms, "Jubilife Pokemon Center entry");
                            pbf_wait(context, 700ms);
                            context.wait_for_all_requests();
                            return;
                        }
                        pbf_wait(context, 300ms);
                    } else {
                        confirmed = 0;
                        bool horizontal = std::abs(result.dx) >= std::abs(result.dy)*9.0/16;
                        double error = horizontal ? result.dx : result.dy*9.0/16;
                        const auto duration = std::abs(error) > 1.0/24 ? 80ms : std::abs(error) > 0.015625 ? 40ms : 16ms;
                        pbf_move_left_joystick(context,
                            {horizontal ? (result.dx > 0 ? 1.0 : -1.0) : 0.0,
                             horizontal ? 0.0 : (result.dy < 0 ? 1.0 : -1.0)}, duration, 0ms);
                        pbf_wait(context, 700ms);
                    }
                    context.wait_for_all_requests();
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Jubilife Center alignment: Door reference not confirmed; entry not attempted.", stream);
            }

        static void tutorial_part_4_navigation(SingleSwitchProgramEnvironment& env, VideoStream& stream, ProControllerContext& context) {
            // What 00.05 does, stepped through on the console before writing it.
            // Every walk is planned on Jubilife's tile grid; every talk stands
            // beside the person, faces them and presses A once.
            static const char* const STEPS[] = {
                "Trainers' School: talk to Barry, who gives the Town Map",
                "Talk to the Poketch Company president on the main road",
                "Get the three coupons from the clowns by the Poke Mart, the TV station and the Poketch Company",
                "Show the coupons to the president for the Poketch",
                "Walk into the Pokemon Center and stand at the PC",
            };
            constexpr size_t STEP_COUNT = sizeof(STEPS) / sizeof(STEPS[0]);
            {
                std::string text = "[00.05] In Jubilife, the steps are:";
                for (size_t i = 0; i < STEP_COUNT; i++) {
                    text += "\n    " + std::to_string(i + 1) + ". " + STEPS[i];
                }
                stream.log(text, COLOR_BLUE);
            }
            auto step = [&](size_t number) {
                std::string text = "[00.05 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + "] " + STEPS[number - 1];
                text += number < STEP_COUNT ? std::string(" -- next: ") + STEPS[number] : std::string(" -- last step");
                stream.log(text, COLOR_BLUE);
                add_wrapped_log(stream.overlay(), "00.05 step " + std::to_string(number) + "/" + std::to_string(STEP_COUNT) + ": " + STEPS[number - 1], COLOR_WHITE);
                if (number < STEP_COUNT) {
                    add_wrapped_log(stream.overlay(), "Next: " + std::string(STEPS[number]), COLOR_GRAY);
                }
            };

            const MapSet& maps = jubilife_maps();
            auto world = [&](const TilePos& pos) { return maps.to_world(MAP_JUBILIFE_CITY, pos); };
            // Jubilife has no trainers; someone in the way only talks.
            std::vector<TrainerSight> trainers;
            auto in_the_way = [&](const std::string&) { finish_dialogue(stream, context, BUTTON_B, "Someone in the way"); };
            std::vector<TilePos> at;
            for (const TilePos& pos : JubilifeCity::after_dawn()) {
                at.push_back(world(pos));
            }

            step(1);
            const TilePos school = world(JubilifeCity::TRAINERS_SCHOOL_DOOR.next(Pokemon::Direction::SOUTH));
            // Doors sit in open plaza, which no wall pins down: end anywhere
            // within a tile of the door's column, and let the door search
            // step sideways.
            auto by_door = [](const TilePos& front) {
                return [front](const TilePos& pos) { return pos.y == front.y && pos.x >= front.x - 1 && pos.x <= front.x + 1; };
            };
            at = walk_route(stream, context, maps, at, by_door(school),
                trainers, in_the_way, "To the Trainers' School");
            enter_door_north(stream, context, "Trainers' School door");
            // Barry stands at the blackboard, a tile west of the door's column:
            // step west, walk north until he stops us, talk.
            route_turn(stream, context, Pokemon::Direction::WEST);
            pbf_move_left_joystick(context, {-1, 0}, route_open_hold(Pokemon::Direction::WEST, 1), 200ms);
            walk_until(stream, context, 0, 1, 3000ms, "to Barry");
            pbf_press_button(context, BUTTON_A, 80ms, 200ms);
            context.wait_for_all_requests();
            {
                ShortDialogWatcher dialog(COLOR_CYAN);
                if (wait_until(stream, context, 2000ms, {{dialog}}) != 0) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Trainers' School: Barry did not answer.", stream);
                }
            }
            // His scene ends with "You obtained a Town Map!", which A closes.
            finish_dialogue(stream, context, BUTTON_A, "Barry");
            // Out: south to the front wall, a tile east (the door's column), out.
            walk_until(stream, context, 0, -1, 4000ms, "to the school's front wall");
            route_turn(stream, context, Pokemon::Direction::EAST);
            pbf_move_left_joystick(context, {1, 0}, route_open_hold(Pokemon::Direction::EAST, 1), 200ms);
            if (walk_until(stream, context, 0, -1, 2500ms, "out of the Trainers' School").stop != WalkStop::TRANSITION) {
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Trainers' School: did not leave by the door.", stream);
            }
            context.wait_for(1500ms);
            at = {school.next(Pokemon::Direction::WEST), school, school.next(Pokemon::Direction::EAST)};

            step(2);
            // The president talks on with A after each last line; B moves his
            // lines on and never starts a new talk.
            const TilePos president = world(JubilifeCity::POKETCH_PRESIDENT);
            talk_to_npc(stream, context, maps, at, president,
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH}, trainers, in_the_way, "Poketch president");
            finish_dialogue(stream, context, BUTTON_B, "Poketch president");

            step(3);
            // Each clown asks a question whose default answer, Yes, is right,
            // and hands over a coupon with an "obtained" message A closes.
            talk_to_npc(stream, context, maps, at, world(JubilifeCity::CLOWN_MART),
                {Pokemon::Direction::SOUTH, Pokemon::Direction::WEST, Pokemon::Direction::EAST},
                trainers, in_the_way, "Clown by the Poke Mart");
            finish_dialogue(stream, context, BUTTON_A, "Clown by the Poke Mart");
            talk_to_npc(stream, context, maps, at, world(JubilifeCity::CLOWN_TV_STATION),
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH, Pokemon::Direction::WEST},
                trainers, in_the_way, "Clown by the TV station");
            finish_dialogue(stream, context, BUTTON_A, "Clown by the TV station");
            talk_to_npc(stream, context, maps, at, world(JubilifeCity::CLOWN_POKETCH_COMPANY),
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH, Pokemon::Direction::WEST},
                trainers, in_the_way, "Clown by the Poketch Company");
            finish_dialogue(stream, context, BUTTON_A, "Clown by the Poketch Company");

            step(4);
            talk_to_npc(stream, context, maps, at, president,
                {Pokemon::Direction::EAST, Pokemon::Direction::SOUTH}, trainers, in_the_way, "Poketch president");
            finish_dialogue(stream, context, BUTTON_B, "Poketch president");

            step(5);
            const TilePos center = world(JubilifeCity::POKEMON_CENTER_DOOR.next(Pokemon::Direction::SOUTH));
            at = walk_route(stream, context, maps, at, by_door(center),
                trainers, in_the_way, "To the Pokemon Center");
            enter_door_north(stream, context, "Pokemon Center door");
            // Inside: north to the counter, three tiles east, north to the PC
            // at the back wall right of the counter.
            walk_until(stream, context, 0, 1, 2500ms, "to the counter");
            route_turn(stream, context, Pokemon::Direction::EAST);
            pbf_move_left_joystick(context, {1, 0}, route_open_hold(Pokemon::Direction::EAST, 3), 200ms);
            walk_until(stream, context, 0, 1, 1500ms, "to the PC");
            context.wait_for_all_requests();
        }
        // ---------------------------------------------------------------------------
        // Segment 00-05 ends
        // Standing in front of the Jubilife City Pokemon Center PC.
        // Player will need to deposit pokemon in the pc and transfer to Pokemon Home
        // and level to 100 before starting the next segment.
        // ---------------------------------------------------------------------------

        // ---------------------------------------------------------------------------
        //  Segment / Checkpoint classes
        // ---------------------------------------------------------------------------

        std::string AutoStory_Segment_00::name()       const { return "00: Intro to Poketech"; }
        std::string AutoStory_Segment_00::start_text() const { return "Start: Game launched, Standing in the player's room after tv stops playing"; }
        std::string AutoStory_Segment_00::end_text()   const { return "End: Starter Pokemon chosen, received, and Pokedex acquired, standing in front of the PC."; }

        void AutoStory_Segment_00::run_segment(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            stats.m_segment++;
            env.update_stats();
            context.wait_for_all_requests();
            env.console.log("Start Segment " + name(), COLOR_ORANGE);

            AutoStory_Checkpoint_0000().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0001().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0002().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0003().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0004().run_checkpoint(env, context, options, stats);
            AutoStory_Checkpoint_0005().run_checkpoint(env, context, options, stats);

            context.wait_for_all_requests();
            env.console.log("End Segment " + name(), COLOR_GREEN);
        }

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
        static void checkpoint_0003(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-002: Pokedex acquisition
                    tutorial_part_2_navigation(env, env.console, context);
                },
                true
            );
        }
        static void checkpoint_0004(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-003: Poketech acquisition
                    tutorial_part_3_navigation(env, env.console, context);
                },
                true
            );

            // Save and advance progress at the catch boundary. Retries of this
            // section must start from the caught-Pokemon save, not the old route.
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    tutorial_part_3_after_catch_navigation(env, env.console, context);
                },
                true
            );
        }
        // DIAGNOSTIC ONLY: Remove before pushing to production.
        void diagnostic_checkpoint_0004_after_catch(
            SingleSwitchProgramEnvironment& env, ProControllerContext& context,
            AutoStoryOptions options, AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    tutorial_part_3_after_catch_navigation(env, env.console, context);
                },
                true
            );
        }

        static void checkpoint_0005(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) {
            checkpoint_reattempt_loop(env, context, options, stats,
                [&](size_t /*attempt*/) {
                    // Segment 00-004: Poketech acquisition
                    tutorial_part_4_navigation(env, env.console, context);
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


        // ========================
        // Checkpoint 0003 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0003::name()       const { return "00.03 - Outside Verity after starter"; }
        std::string AutoStory_Checkpoint_0003::start_text() const { return AutoStory_Checkpoint_0002().end_text(); }
        std::string AutoStory_Checkpoint_0003::end_text()   const { return "Standing in front of The professor after recieving the Pokedex"; }

        void AutoStory_Checkpoint_0003::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0003(env, context, options, stats);
        }
        // ========================
        // Checkpoint 0004 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0004::name()       const { return "00.04 - After Pokedex"; }
        std::string AutoStory_Checkpoint_0004::start_text() const { return AutoStory_Checkpoint_0003().end_text(); }
        std::string AutoStory_Checkpoint_0004::end_text()   const { return "Standing in in jubilife after speaking to dawn. Next: give town map to barry"; }

        void AutoStory_Checkpoint_0004::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0004(env, context, options, stats);
        }
        // ========================
        // Checkpoint 0005 Wrapper
        // ========================

        std::string AutoStory_Checkpoint_0005::name()       const { return "00.05 - Jubilife After Dawn"; }
        std::string AutoStory_Checkpoint_0005::start_text() const { return AutoStory_Checkpoint_0004().end_text(); }
        std::string AutoStory_Checkpoint_0005::end_text()   const { return "Standing in front of PC. Recieved the Poketech. Ready to deposit starter"; }

        void AutoStory_Checkpoint_0005::run_checkpoint(
            SingleSwitchProgramEnvironment& env,
            ProControllerContext& context,
            AutoStoryOptions options,
            AutoStoryStats& stats
        ) const {
            checkpoint_0005(env, context, options, stats);
        }
        
        }
    }
}
