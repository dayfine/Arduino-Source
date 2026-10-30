/*  BDSP AutoStory - Story Battles
 *  Fights story and trainer battles with the best move, Potions when HP runs
 *  low, and the dialogue after them.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <chrono>

#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/VisualDetectors/ImageMatchDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_EndBattleDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"

#include "PokemonBDSP_AutoStory_MoveSelect.h"
#include "PokemonBDSP_Autostory_Segment_01_tools.h"
#include "PokemonBDSP_AutoStory_StoryBattle.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // BattleDialogWatcher
            // ---------------------------------------------------------------------------

            BattleDialogWatcher::BattleDialogWatcher(Color color)
                : DetectorToFinder(
                    "BattleDialogWatcher",
                    std::chrono::milliseconds(250),
                    color
                )
            {
            }

            // ---------------------------------------------------------------------------
            // Starly battle
            // ---------------------------------------------------------------------------

            void fight_starly(VideoStream& stream, ProControllerContext& context) {
                stream.log("[AutoStory] Starting Starly battle...", COLOR_BLUE);

                // Declare OUTSIDE the loop so accumulated detector state
                // (trigger counts, black-screen transitions) is preserved across frames.
                // STARTER type: player has no Pokeballs, so ball-count areas won't
                // be white. STARTER skips that check and only verifies status bars
                // and the Fight button color.
                BattleMenuWatcher battle_menu(BattleType::STARTER, COLOR_YELLOW);
                EndBattleWatcher  end_battle;

                while (true) {
                    context.wait_for_all_requests();

                    // Phase 1: wait for either battle menu or battle end.
                    int ret = run_until<ProControllerContext>(
                        stream, context,
                        [](ProControllerContext& ctx) {
                            pbf_mash_button(ctx, BUTTON_B, 120000ms);
                        },
            {
                {battle_menu},
                {end_battle},
            }
            );

                    switch (ret) {
                    case 0:
                    {
                        stream.log("[AutoStory] Battle menu detected. Selecting move...", COLOR_BLUE);

                        // Select Fight, then move slot 1.
                        pbf_press_button(context, BUTTON_A, 80ms, 500ms);
                        pbf_press_button(context, BUTTON_A, 80ms, 3000ms);
                        // Loop back - mash B through animations until next menu or end.
                        break;
                    }
                    case 1:
                    {
                        stream.log("[AutoStory] Starly battle complete.", COLOR_GREEN);

                        // Mash through XP / level-up / dialog screens back to overworld.
                        OverworldWatcher overworld;
                        int cleanup_ret = run_until<ProControllerContext>(
                            stream, context,
                            [](ProControllerContext& ctx) {
                                pbf_mash_button(ctx, BUTTON_B, 120000ms);
                            },
                            { {overworld} }
                        );
                        if (cleanup_ret != 0) {
                            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                "Starly battle: Overworld cleanup timed out.", stream);
                        }
                        return;
                    }
                    default:
                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "Starly battle timed out.", stream);
                    }
                }
            }

            void confirm_battle_overworld(VideoStream& stream, ProControllerContext& context,
                const std::string& label) {
                OverworldDetector overworld;
                ShortDialogDetector dialogue(COLOR_YELLOW);
                BlackScreenDetector black;
                VideoOverlaySet overlays(stream.overlay());
                overworld.make_overlays(overlays);
                dialogue.make_overlays(overlays);
                black.make_overlays(overlays);
                const auto deadline = std::chrono::steady_clock::now() + 180s;
                auto quiet_since = std::chrono::steady_clock::now();
                bool finished = false;
                while (std::chrono::steady_clock::now() < deadline) {
                    const auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label + ": No video frame.", stream);
                    const auto now = std::chrono::steady_clock::now();
                    if (dialogue.detect(frame) || black.detect(frame) || !overworld.detect(frame)) {
                        quiet_since = now;
                    } else if (now - quiet_since >= 4s) {
                        finished = true;
                        break;
                    }
                    pbf_mash_button(context, BUTTON_B, 100ms);
                    context.wait_for_all_requests();
                }
                if (!finished) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    label + ": Dialogue-free overworld not confirmed for four seconds.", stream);
                context.wait_for_all_requests();
            }

            // Our HP bar in the battle's bottom-left panel, as a fraction; -1 if
            // no bar is there. Measured on 1920x1080 frames: the bar runs from
            // x 88 to 466 on rows 961 to 965.
            static double battle_player_hp(const ImageViewRGB32& frame) {
                const ImageViewRGB32 bar = extract_box_reference(frame, ImageFloatBox{0.046, 0.890, 0.197, 0.004});
                size_t filled = 0;
                for (size_t x = 0; x < bar.width(); x++) {
                    size_t color = 0;
                    for (size_t y = 0; y < bar.height(); y++) {
                        const uint32_t p = bar.pixel(x, y);
                        const int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
                        const bool green = g > r + 30 && g > b + 20;
                        const bool yellow = r > 150 && g > 120 && b < 100;
                        const bool red = r > 150 && r > g + 50 && r > b + 50;
                        color += green || yellow || red;
                    }
                    filled += color > bar.height() / 2;
                }
                return bar.width() && filled ? (double)filled / bar.width() : -1;
            }

            enum class BattleHeal { NOT_NEEDED, USED, NONE_LEFT };

            // At the battle menu: if our HP is under `heal_below`, use a Potion
            // from the Bag on the lead, which takes the turn. Otherwise the
            // battle menu is left up.
            static BattleHeal battle_heal_if_low(VideoStream& stream, ProControllerContext& context,
                BattleType battle_type, double heal_below, const std::string& label) {
                context.wait_for_all_requests();
                double hp = -1;
                for (size_t check = 0; check < 5 && hp < 0; check++) {
                    const auto frame = stream.video().snapshot();
                    if (frame) hp = battle_player_hp(*frame.frame);
                    if (hp < 0) pbf_wait(context, 150ms);
                    context.wait_for_all_requests();
                }
                stream.log("[" + label + "] Our HP bar reads " + std::to_string((int)(hp * 100)) + "%.");
                if (hp < 0 || hp >= heal_below) return BattleHeal::NOT_NEEDED;
                BattleMenuDetector commands(battle_type);
                bool on_bag = false;
                for (size_t check = 0; check < 30 && !on_bag; check++) {
                    context.wait_for_all_requests();
                    const auto frame = stream.video().snapshot();
                    const BattleMenuOption selected = frame ? commands.selected_option(frame) : BattleMenuOption::UNKNOWN;
                    if (selected == BattleMenuOption::BAG) on_bag = true;
                    else if (selected == BattleMenuOption::UNKNOWN) pbf_wait(context, 150ms);
                    else pbf_press_dpad(context, (int)selected < (int)BattleMenuOption::BAG ? DPAD_DOWN : DPAD_UP, 80ms, 300ms);
                }
                if (!on_bag) {
                    stream.log("[" + label + "] Could not reach the Bag command; fighting on.", COLOR_ORANGE);
                    return BattleHeal::NONE_LEFT;
                }
                pbf_press_button(context, BUTTON_A, 80ms, 2000ms);
                if (!find_bag_item_in_open_bag(stream, context, "potion", 1)) {
                    stream.log("[" + label + "] No Potion left; fighting on.", COLOR_ORANGE);
                    pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
                    pbf_press_button(context, BUTTON_B, 80ms, 1500ms);
                    context.wait_for_all_requests();
                    return BattleHeal::NONE_LEFT;
                }
                // Potion, then the lead in the party list, then Use.
                stream.log("[" + label + "] Using a Potion on the lead.", COLOR_BLUE);
                pbf_press_button(context, BUTTON_A, 80ms, 1500ms);
                pbf_press_button(context, BUTTON_A, 80ms, 1500ms);
                pbf_press_button(context, BUTTON_A, 80ms, 2500ms);
                context.wait_for_all_requests();
                return BattleHeal::USED;
            }

            static void fight_using_move_list(
                VideoStream& stream, ProControllerContext& context,
                BattleType battle_type, const std::vector<size_t>& move_slots,
                const std::string& label, bool confirm_default_target, bool post_battle_dialogue_cleanup,
                bool doubles = false, bool switch_on_faint = false
            ) {
                if (move_slots.empty() || std::any_of(move_slots.begin(),move_slots.end(),
                    [](size_t slot) { return slot>4; })) {
                    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        label+": Supply a nonempty move list containing only positions 0 (best) through 4.",stream);
                }
                size_t move_index=0;
                // The first Fight menu starts at move 1. Later turns retain
                // this Pokemon's previous selection; a new battle resets it.
                size_t current_move=1;
                stream.log(
                    "[AutoStory] Starting " + label + " battle...",
                    COLOR_BLUE
                );

                // Scripted battles wait on the menu detector below, so no
                // extra fixed delay is needed after a caller confirms it.
                // Preserve the existing early-game transition delay.
                if (!confirm_default_target) pbf_wait(context,5000ms);

                bool battle_menu_seen = false;
                bool out_of_potions = false;
                size_t potions_used = 0;
                size_t turns = 0;
                size_t faint_switches = 0;
                const bool watch_learn_move = confirm_default_target || doubles || switch_on_faint;
                EndBattleWatcher end_battle;

                while (true) {

                    context.wait_for_all_requests();

                    BattleMenuWatcher battle_menu(battle_type);
                    // Reuse the farming handlers' move-learning prompt recovery.
                    SelectionArrowFinder learn_move(stream.overlay(),{0.50,0.62,0.40,0.18},COLOR_YELLOW);

                    int ret = run_until<ProControllerContext>(
                        stream,
                        context,
                        [&](ProControllerContext& context) {
                            // Clear narration with B while watching for commands.
                            // A here can open Fight before selection begins.
                            // With faint switching, a quiet 30 s means the party
                            // screen after a faint, which B cannot leave.
                            pbf_mash_button(context,BUTTON_B,switch_on_faint ? 30000ms : 120000ms);
                        },
                        {
                            {battle_menu},
                            battle_menu_seen
                                ? PeriodicInferenceCallback{end_battle}
                                : PeriodicInferenceCallback{},
                            watch_learn_move ? PeriodicInferenceCallback{learn_move} : PeriodicInferenceCallback{},
                        }
                    );

                    switch (ret) {

                        // -------------------------------------------------
                        // Battle menu detected.
                        // -------------------------------------------------
                    case 0:
                        battle_menu_seen = true;
                        // Early battles take a few turns; this many means the chosen
                        // move is not being used (no PP, a misread menu).
                        if (++turns > 25) {
                            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                label + ": still at the battle menu after 25 turns.", stream);
                        }

                        stream.log(
                            "[AutoStory] Battle menu detected.",
                            COLOR_BLUE
                        );

                        // A trainer battle we may lose: a Potion when HP is low.
                        if (battle_type == BattleType::TRAINER && !out_of_potions && potions_used < 4) {
                            const BattleHeal heal = battle_heal_if_low(stream, context, battle_type, 0.35, label);
                            if (heal == BattleHeal::USED) {
                                potions_used++;
                                break;
                            }
                            out_of_potions = heal == BattleHeal::NONE_LEFT;
                        }
                        stream.log("["+label+"] Move list position "+std::to_string(move_index+1)+
                            ": use move "+std::to_string(move_slots[move_index])+".");
                        select_battle_move_slot(stream, context, battle_type, move_slots[move_index], label,
                            confirm_default_target,current_move);
                        current_move=move_slots[move_index];
                        // A double battle asks for the target next; B would
                        // back out of it. The default target is an opponent.
                        if (doubles) {
                            pbf_wait(context,1200ms);
                            pbf_press_button(context,BUTTON_A,80ms,1500ms);
                        }
                        // Advance once per move selection; retain the final entry.
                        if (move_index+1<move_slots.size()) move_index++;

                        break;

                        // -------------------------------------------------
                        // End battle detected.
                        // -------------------------------------------------
                    case 1: {
                        stream.log(
                            "[AutoStory] " + label + " battle complete.",
                            COLOR_GREEN
                        );

                        // EndBattleWatcher has confirmed the completed exit transition.
                        // Ordinary trainers need no scene-wide dialogue-free gate.
                        if (!post_battle_dialogue_cleanup) {
                            context.wait_for_all_requests();
                            return;
                        }
                        // Story NPCs and gym leaders opt into the existing cleanup.
                        OverworldWatcher overworld;
                        int cleanup_ret = run_until<ProControllerContext>(stream, context,
                            [](ProControllerContext& ctx) {
                                pbf_mash_button(ctx, BUTTON_B, 120000ms);
                            }, {{overworld}});
                        if (cleanup_ret != 0) {
                            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                                label + ": Post-battle cleanup timed out.", stream);
                        }
                        context.wait_for_all_requests();
                        confirm_battle_overworld(stream, context, label + " cleanup");
                        return;
                    }

                    case 2:
                        stream.log("["+label+"] Declining move learning; preserving scripted move positions.");
                        pbf_move_right_joystick(context,{0,-1},160ms,840ms);
                        pbf_press_button(context,BUTTON_ZL,160ms,840ms);
                        break;

                    default:
                        // After a faint the game shows the party: Down to the
                        // next Pokemon, A, then A on "Send out" (by hand,
                        // 2026-10-29 against Roark).
                        if (switch_on_faint && faint_switches < 5) {
                            faint_switches++;
                            stream.log("[" + label + "] No battle menu for 30 s: a faint; sending out the next Pokemon.", COLOR_ORANGE);
                            pbf_press_dpad(context, DPAD_DOWN, 80ms, 500ms);
                            pbf_press_button(context, BUTTON_A, 80ms, 1200ms);
                            pbf_press_button(context, BUTTON_A, 80ms, 2000ms);
                            break;
                        }
                        stream.log(
                            "[AutoStory] " + label + " battle timeout.",
                            COLOR_RED
                        );

                        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                            "[AutoStory] " + label + " battle timeout.", stream);
                    }
                }
            }

            // Preserve the existing early-game callers and their B text cleanup.
            void fight_using_move_slot(VideoStream& stream, ProControllerContext& context,
                BattleType battle_type, size_t move_slot, const std::string& label) {
                fight_using_move_list(stream,context,battle_type,{move_slot},label,false,true);
            }

            void clear_post_battle_dialogue(VideoStream& stream, ProControllerContext& context,
                Button button, Milliseconds quiet_period,
                const char* instruction_card) {
                context.wait_for_all_requests();
                std::unique_ptr<ImageMatchDetector> card;
                if (instruction_card) card = std::make_unique<ImageMatchDetector>(
                    std::make_shared<ImageRGB32>(RESOURCE_PATH()+instruction_card),
                    ImageFloatBox{.04,.32,.92,.34},30.0);
                ShortDialogDetector dialogue(COLOR_YELLOW);
                BlackScreenDetector black;
                VideoOverlaySet overlays(stream.overlay());
                dialogue.make_overlays(overlays);
                black.make_overlays(overlays);
                const auto deadline = std::chrono::steady_clock::now() + 120s;
                auto absent_since = std::chrono::steady_clock::now();
                while (std::chrono::steady_clock::now() < deadline) {
                    const auto frame = stream.video().snapshot();
                    if (!frame) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                        "Post-battle dialogue: No video frame.", stream);
                    const auto now = std::chrono::steady_clock::now();
                    const bool dialogue_visible = dialogue.detect(*frame.frame);
                    const bool card_visible = card && card->detect(*frame.frame);
                    if (dialogue_visible || card_visible || black.detect(*frame.frame)) absent_since = now;
                    else if (now - absent_since >= quiet_period) {
                        stream.log("[Post-battle dialogue] " + std::to_string(quiet_period.count()) +
                            "ms without dialogue confirmed.");
                        return;
                    }
                    // A can interact again once dialogue disappears. Only confirm
                    // visible prompts; B retains the existing post-battle behavior.
                    if (card_visible) pbf_press_button(context, BUTTON_B, 80ms, 100ms);
                    else if (button == BUTTON_A && !dialogue_visible) pbf_wait(context, 100ms);
                    else pbf_mash_button(context, button, 100ms);
                    context.wait_for_all_requests();
                }
                OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                    "Post-battle dialogue: Three clear seconds not confirmed.", stream);
            }

            void fight_trainer_using_moves(VideoStream& stream, ProControllerContext& context,
                const std::vector<size_t>& moves, const std::string& label, bool post_battle_dialogue_cleanup) {
                fight_using_move_list(stream,context,BattleType::TRAINER,moves,label,true,post_battle_dialogue_cleanup);
            }

            void fight_trainer_using_moves(VideoStream& stream, ProControllerContext& context,
                size_t move, const std::string& label, bool post_battle_dialogue_cleanup) {
                fight_trainer_using_moves(stream,context,std::vector<size_t>{move},label,post_battle_dialogue_cleanup);
            }

            void fight_trainer_using_move_slot(VideoStream& stream, ProControllerContext& context,
                size_t move_slot, const std::string& label) {
                fight_using_move_slot(stream,context,BattleType::TRAINER,move_slot,label);
            }

            void fight_story_battle(VideoStream& stream, ProControllerContext& context, const std::string& label, bool doubles) {
                // The best move each turn; the next Pokemon after a faint; no new moves.
                fight_using_move_list(stream, context, BattleType::TRAINER, {0}, label, false, true, doubles, true);
            }

            void fight_trainer(VideoStream& stream, ProControllerContext& context, const std::string& label) {
                // Move 0: the best move on the menu each turn, whatever the starter.
                fight_using_move_slot(stream, context, BattleType::TRAINER, 0, label);
            }


        }
    }
}
