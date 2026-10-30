/*  BDSP AutoStory - Wild Encounters
 *  Finds wild Pokemon in tall grass to train on or catch, and walks through
 *  grass running from them.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>
#include <cmath>

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_EndBattleDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_ReceivePokemonDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"

#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_StoryBattle.h"
#include "PokemonBDSP_Autostory_Segment_01_tools.h"
#include "PokemonBDSP_AutoStory_WildEncounters.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // Tristan battle
            // ---------------------------------------------------------------------------
            // Two grass rows at 240 ms each, with room to spare: the wall stops us.
            static constexpr Milliseconds CATCH_GRASS_WALK = 900ms;

            static void encounter_one_wild_pokemon(VideoStream& stream, ProControllerContext& context) {
                context.wait_for_all_requests();
                pbf_wait(context, 1000ms);

                bool encounter_found = false;
                const auto deadline = std::chrono::steady_clock::now() + 180s;

                // Walk up and down the catch grass until a battle starts. Its three rows
                // sit between tree walls, so each walk ends at a wall, on the same tile
                // every time, after two new grass tiles. Short sideways steps drift and
                // can rock across one tile edge without entering new grass.
                while (!encounter_found && std::chrono::steady_clock::now() < deadline) {
                    BlackScreenWatcher battle_start;
                    BattleMenuWatcher battle_menu(BattleType::STANDARD, COLOR_YELLOW);
                    BattleDialogWatcher battle_dialog(COLOR_YELLOW);

                    route_turn(stream, context, Pokemon::Direction::NORTH);
                    pbf_move_left_joystick(context, {0, 1}, CATCH_GRASS_WALK, 200ms);
                    context.wait_for_all_requests();

                    int ret = wait_until(
                        stream,
                        context,
                        1500ms,
                        {
                            {battle_start},
                            {battle_menu},
                            {battle_dialog},
                        }
                    );

                    if (ret >= 0) {
                        encounter_found = true;
                        break;
                    }

                    route_turn(stream, context, Pokemon::Direction::SOUTH);
                    pbf_move_left_joystick(context, {0, -1}, CATCH_GRASS_WALK, 200ms);
                    context.wait_for_all_requests();

                    ret = wait_until(
                        stream,
                        context,
                        1500ms,
                        {
                            {battle_start},
                            {battle_menu},
                            {battle_dialog},
                        }
                    );

                    if (ret >= 0) {
                        encounter_found = true;
                        break;
                    }
                }

                if (!encounter_found) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Wild encounter: walking up and down the grass found no battle within three minutes.", stream);
                stream.log("[AutoStory] Wild battle detected!", COLOR_GREEN);

                // Wait until the actual battle menu appears.
                BattleMenuWatcher confirm_menu(BattleType::STANDARD);

                int menu_ret = wait_until(
                    stream,
                    context,
                    15s,
                    {
                        {confirm_menu},
                    }
                );

                if (menu_ret < 0) {
                    stream.log(
                        "[AutoStory] Failed to detect battle menu.",
                        COLOR_RED
                    );
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "[AutoStory] Failed to detect battle menu.", stream);
                }

                stream.log("[AutoStory] Battle menu detected.", COLOR_GREEN);

                context.wait_for_all_requests();
                pbf_wait(context, 500ms);

            }

            void battle_1_pokemon(VideoStream& stream, ProControllerContext& context) {
                stream.log("[Training] Finding one wild Pokemon using the catch encounter wiggle.", COLOR_BLUE);
                encounter_one_wild_pokemon(stream, context);
                // The best move on the menu, whatever the starter.
                fight_using_move_slot(stream, context, BattleType::STANDARD, 0, "Training");
                context.wait_for_all_requests();
                stream.log("[Training] One wild battle completed.", COLOR_GREEN);
            }

            // ---------------------------------------------------------------------------
            // Catch 1 Pokemon
            // ---------------------------------------------------------------------------

            void catch_1_pokemon(
                VideoStream& stream,
                ProControllerContext& context
            ) {
                stream.log("[AutoStory] Starting catch_1_pokemon helper...", COLOR_BLUE);

                encounter_one_wild_pokemon(stream, context);

                // Catch the pokemon.
                stream.log("[AutoStory] Throwing Poke Ball (manual bag open)...", COLOR_BLUE);
                ReceivePokemonDetector received_pokemon;
                // Preserve exit-transition state from the ball outcome through
                // receipt/experience cleanup; do not recreate it between phases.
                EndBattleWatcher battle_ended;
                bool caught = false;
                bool exit_seen = false;
                for (int throw_attempt = 0; throw_attempt < 10; throw_attempt++) {
                    // Open bag from battle menu (X), confirm first slot (ZL).
                    pbf_press_button(context, BUTTON_X, 160ms, 840ms);
                    context.wait_for_all_requests();
                    pbf_press_button(context, BUTTON_ZL, 160ms, 1000ms);
                    context.wait_for_all_requests();

                    // Wait to see if the battle menu comes back (ball failed)
                    // or if the battle ends (caught or fainted).
                    BattleMenuWatcher menu_back(BattleType::STANDARD, COLOR_YELLOW);
                    int throw_ret = run_until<ProControllerContext>(
                        stream, context,
                        [](ProControllerContext& ctx) {
                            pbf_mash_button(ctx, BUTTON_B, 120000ms);
                        },
                        {{menu_back}, {received_pokemon}, {battle_ended}}
                    );

                    if (throw_ret == 1 || (throw_ret == 2 && received_pokemon.received())) {
                        caught = true;
                        exit_seen = throw_ret == 2;
                        stream.log("[AutoStory] Pokemon receipt verified.", COLOR_GREEN);
                        break;
                    }
                    if (throw_ret < 0 || throw_ret == 2) {
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Catch failed: No verified Pokemon receipt before battle ended or timed out.", stream);
                    }
                    stream.log(
                        "[AutoStory] Ball failed, retrying (attempt " + std::to_string(throw_attempt + 2) + ")...",
                        COLOR_ORANGE
                    );
                }

                context.wait_for_all_requests();

                if (!caught) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Catch failed: Exhausted ten ball attempts.", stream);
                }
                // Receipt alone is not an overworld return: clear experience,
                // automatic move learning, dex registration and nickname screens
                // until the same completed-black-transition watcher used in battles fires.
                if (!exit_seen) {
                    stream.log("[Catch cleanup] Receipt confirmed; waiting for battle exit transition.");
                    const int cleanup_ret = run_until<ProControllerContext>(stream, context,
                        [](ProControllerContext& ctx) { pbf_mash_button(ctx, BUTTON_B, 120000ms); },
                        {{battle_ended}});
                    if (cleanup_ret != 0) {
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Catch cleanup: Battle exit transition did not finish.", stream);
                    }
                }
                context.wait_for_all_requests();
                stream.log("[Catch cleanup] Battle exit transition confirmed; checking stable overworld.");
                confirm_battle_overworld(stream, context, "Catch cleanup");
                stream.log("[Catch cleanup] Verified receipt, completed battle exit, and four seconds of clear overworld.", COLOR_GREEN);
            }

            // ---------------------------------------------------------------------------
            // Generic Movement Helper - Walk through grass in specified direction for
            // specified steps, attempting to run from any battles that are encountered.
            // ---------------------------------------------------------------------------
            void walk_through_grass(
                VideoStream& stream, ProControllerContext& context,
                int8_t x, int8_t y, size_t steps, const std::string& direction_name,
                BattleType battle_type, Milliseconds movement_duration,
                OverworldMatchResult (*destination)(const ImageRGB32&)
            ) {
                size_t movements = 0, confirmations = 0;
                while (true) {
                    context.wait_for_all_requests();
                    // Destination checks happen before input and after each step/escape.
                    // With no matcher, retain the original fixed-step behavior.
                    VideoOverlaySet overlays(stream.overlay());
                    OverworldMatchResult location;
                    if (destination) {
                        const auto frame = stream.video().snapshot();
                        if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Grass destination: No video frame.", stream);
                        location = destination(*frame.frame);
                        for (const auto& landmark : location.landmarks) {
                            overlays.add(COLOR_CYAN, landmark.search);
                            if (landmark.accepted) overlays.add(COLOR_GREEN, landmark.found);
                            stream.log("[Grass destination] " + direction_name + " " + landmark.name +
                                " score=" + std::to_string(landmark.score) +
                                " margin=" + std::to_string(landmark.margin) +
                                " accepted=" + std::to_string(landmark.accepted));
                        }
                        stream.log("[Grass destination] located=" + std::to_string(location.located) +
                            " at_target=" + std::to_string(location.at_target) +
                            " screen-relative offset=(" + std::to_string(location.dx) +
                            "," + std::to_string(location.dy) + ")");
                        if (location.at_target) {
                            if (++confirmations >= 3) {
                                stream.log("[Grass] Destination confirmed; stopped " + direction_name +
                                    " after " + std::to_string(movements) + "/" + std::to_string(steps) + " movements.", COLOR_GREEN);
                                return;
                            }
                            pbf_wait(context, 250ms);
                            continue;
                        }
                        confirmations = 0;
                    }
                    if (movements >= steps) {
                        if (destination) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Grass destination not confirmed within movement limit: " + direction_name, stream);
                        stream.log("[Grass] Completed " + direction_name + " (" + std::to_string(steps) + " steps).", COLOR_GREEN);
                        return;
                    }
                    int8_t move_x=x, move_y=y;
                    Milliseconds duration=movement_duration;
                    if (destination && location.located && !location.coarse &&
                        std::abs(location.dx)<=0.10 && std::abs(location.dy)<=0.12) {
                        // Small corrections prevent stepping past a narrow stop target.
                        const bool horizontal=std::abs(location.dx)>=std::abs(location.dy)*9.0/16;
                        const double error=horizontal ? location.dx : location.dy*9.0/16;
                        duration=std::min(duration, std::abs(error)>0.03125 ? 80ms :
                            std::abs(error)>0.015625 ? 40ms : 16ms);
                        move_x=horizontal ? (location.dx>0 ? 1 : -1) : 0;
                        move_y=horizontal ? 0 : (location.dy<0 ? 1 : -1);
                        stream.log("[Grass destination] Correcting " + std::string(horizontal ? "horizontal" : "vertical") +
                            " axis for " + std::to_string(duration.count()) + "ms.");
                    }
                    // Each movement gets fresh watcher state. A completed white
                    // transition from a prior encounter must not trigger again.
                    BlackScreenWatcher battle_start;
                    WhiteScreenOverWatcher white_screen(COLOR_YELLOW, {0.1, 0.1, 0.8, 0.8});
                    BattleMenuWatcher battle_menu(battle_type, COLOR_YELLOW);
                    BattleDialogWatcher battle_dialog(COLOR_YELLOW);
                    pbf_move_left_joystick(context, {(double)move_x, (double)move_y}, duration, 0ms);
                    movements++;
                    context.wait_for_all_requests();
                    int ret = wait_until(stream, context, 2500ms,
                        {{white_screen}, {battle_start}, {battle_dialog}, {battle_menu}});
                    if (ret >= 0) {
                        confirmations = 0;
                        stream.log("[Grass] " + direction_name + " step " + std::to_string(movements) +
                            "/" + std::to_string(steps) + ": encounter trigger=" + std::to_string(ret) +
                            "; using shared confirmed-Run escape.");
                        escape_wild_battle(stream, context, battle_type);
                        context.wait_for_all_requests();
                        stream.log("[Grass] Escape confirmed; resuming " + direction_name + ".");
                    }
                }
            }


        }
    }
}
