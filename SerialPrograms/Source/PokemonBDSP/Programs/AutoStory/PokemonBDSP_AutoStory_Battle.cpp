/*  BDSP AutoStory - Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "CommonTools/VisualDetectors/BlackScreenDetector.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_BattleMenuDetector.h"
#include "PokemonBDSP/Inference/Battles/PokemonBDSP_EndBattleDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_DialogDetector.h"
#include "PokemonBDSP/Inference/PokemonBDSP_SelectionArrow.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_BattleMenuReader.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_Bag.h"
#include "PokemonBDSP_AutoStory_BattleMoves.h"
#include "PokemonBDSP_AutoStory_Battle.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


BattleDialogWatcher::BattleDialogWatcher(Color color)
    : DetectorToFinder(
        "BattleDialogWatcher",
        std::chrono::milliseconds(250),
        color
    )
{
}

namespace{

//  On the battle menu: the cursor onto Battle (the command list's first
//  row), confirmed on three frames, then A.
void choose_battle_command(VideoStream& stream, ProControllerContext& context, BattleType battle_type, const std::string& label){
    BattleMenuDetector commands(battle_type);
    VideoOverlaySet overlays(stream.overlay());
    commands.make_overlays(overlays);
    overlays.add(COLOR_GREEN, {0.90, 0.48, 0.075, 0.505});
    size_t confirmed = 0;
    for (size_t check = 0; check < 30 && confirmed < 3; check++){
        context.wait_for_all_requests();
        auto frame = stream.video().snapshot();
        BattleMenuOption selected = frame ? battle_menu_selected_option(commands, frame) : BattleMenuOption::UNKNOWN;
        stream.log("[" + label + "] Command cursor=" + std::to_string((int)selected + 1) + "; target Battle=1.");
        if (selected == BattleMenuOption::BATTLE){
            if (++confirmed < 3){
                pbf_wait(context, 100ms);
            }
        }else if (selected == BattleMenuOption::UNKNOWN){
            confirmed = 0;
            pbf_wait(context, 150ms);
        }else{
            confirmed = 0;
            pbf_press_dpad(context, DPAD_UP, 80ms, 250ms);
        }
    }
    if (confirmed < 3){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": Could not confirm Battle command; A not pressed.", stream);
    }
    pbf_press_button(context, BUTTON_A, 80ms, 500ms);
}

//  Watching the move menu settle: how many frames in a row showed the same
//  number of moves, and how many had the cursor on the target.
struct MoveMenuWatch{
    size_t previous_move_count = 0;
    size_t stable_move_count = 0;
    size_t confirmed = 0;
    //  The move to use, 1-based; 0 until the menu has settled and the best is picked.
    size_t target = 0;
};

//  One look at the move menu. Returns whether the target is confirmed
//  under the cursor; otherwise moves the cursor toward it or waits.
bool move_menu_step(VideoStream& stream, ProControllerContext& context, MoveMenuWatch& watch, const std::string& label){
    context.wait_for_all_requests();
    auto frame = stream.video().snapshot();
    const BattleMoveMenuState moves = frame ? detect_battle_move_menu(frame) : BattleMoveMenuState{};
    const int slot = (int)moves.selected_slot;
    VideoOverlaySet move_overlays(stream.overlay());
    for (size_t i = 0; i < moves.move_count; i++){
        move_overlays.add(moves.selected_slot == i + 1 ? COLOR_GREEN : COLOR_YELLOW, moves.buttons[i]);
    }
    stream.log("[" + label + "] Available moves=" + std::to_string(moves.move_count) +
        "; selected slot=" + std::to_string(slot) + "/4; target=" + std::to_string(watch.target) + ".");
    if (moves.move_count == watch.previous_move_count && moves.move_count > 0){
        watch.stable_move_count++;
    }else{
        watch.previous_move_count = moves.move_count;
        watch.stable_move_count = 1;
        watch.confirmed = 0;
    }
    // Pick the best move once the menu has settled.
    if (watch.target == 0 && watch.stable_move_count >= 3 && frame){
        watch.target = pick_best_move(stream, *frame.frame, moves, label);
    }
    if (moves.move_count > 0 && moves.move_count < watch.target && watch.stable_move_count >= 3){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": Move slot " + std::to_string(watch.target) + " is absent; no move confirmed.", stream);
    }
    if (slot == (int)watch.target && watch.stable_move_count >= 3){
        if (++watch.confirmed >= 3){
            return true;
        }
        pbf_wait(context, 100ms);
    }else{
        watch.confirmed = 0;
        if (slot == 0 || slot == (int)watch.target){
            pbf_wait(context, 150ms);
        }else{
            pbf_press_dpad(context, slot < (int)watch.target ? DPAD_DOWN : DPAD_UP, 80ms, 250ms);
        }
    }
    context.wait_for_all_requests();
    return false;
}

//  On the battle menu: Battle, then the best move on the move menu once
//  it has settled, confirmed under the cursor, then A.
void use_best_move(VideoStream& stream, ProControllerContext& context, BattleType battle_type, const std::string& label){
    choose_battle_command(stream, context, battle_type, label);
    MoveMenuWatch watch;
    for (size_t check = 0; check < 30; check++){
        if (move_menu_step(stream, context, watch, label)){
            stream.log("[" + label + "] Move " + std::to_string(watch.target) + " confirmed; using it.");
            pbf_press_button(context, BUTTON_A, 80ms, 3000ms);
            return;
        }
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": Could not confirm move " + std::to_string(watch.target) + "; A not pressed.", stream);
}

// Our HP bar in the battle's bottom-left panel, as a fraction; -1 if
// no bar is there. Measured on 1920x1080 frames: the bar runs from
// x 88 to 466 on rows 961 to 965.
double battle_player_hp(const ImageViewRGB32& frame){
    const ImageViewRGB32 bar = extract_box_reference(frame, ImageFloatBox{0.046, 0.890, 0.197, 0.004});
    size_t filled = 0;
    for (size_t x = 0; x < bar.width(); x++){
        size_t color = 0;
        for (size_t y = 0; y < bar.height(); y++){
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

//  Our HP at the battle menu, from up to five looks; -1 if unread.
double read_battle_hp(VideoStream& stream, ProControllerContext& context){
    context.wait_for_all_requests();
    double hp = -1;
    for (size_t check = 0; check < 5 && hp < 0; check++){
        const auto frame = stream.video().snapshot();
        if (frame){
            hp = battle_player_hp(*frame.frame);
        }
        if (hp < 0){
            pbf_wait(context, 150ms);
        }
        context.wait_for_all_requests();
    }
    return hp;
}

//  On the battle menu: the cursor onto Bag. Whether it got there.
bool cursor_to_bag(VideoStream& stream, ProControllerContext& context, BattleType battle_type){
    BattleMenuDetector commands(battle_type);
    for (size_t check = 0; check < 30; check++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const BattleMenuOption selected = frame ? battle_menu_selected_option(commands, frame) : BattleMenuOption::UNKNOWN;
        if (selected == BattleMenuOption::BAG){
            return true;
        }
        if (selected == BattleMenuOption::UNKNOWN){
            pbf_wait(context, 150ms);
        }else{
            pbf_press_dpad(context, (int)selected < (int)BattleMenuOption::BAG ? DPAD_DOWN : DPAD_UP, 80ms, 300ms);
        }
    }
    return false;
}

enum class BattleHeal{ NOT_NEEDED, USED, NONE_LEFT };

// At the battle menu: if our HP is under `heal_below`, use a Potion
// from the Bag on the lead, which takes the turn. Otherwise the
// battle menu is left up.
BattleHeal battle_heal_if_low(VideoStream& stream, ProControllerContext& context,
    BattleType battle_type, double heal_below, const std::string& label){
    const double hp = read_battle_hp(stream, context);
    stream.log("[" + label + "] Our HP bar reads " + std::to_string((int)(hp * 100)) + "%.");
    if (hp < 0 || hp >= heal_below){
        return BattleHeal::NOT_NEEDED;
    }
    if (!cursor_to_bag(stream, context, battle_type)){
        stream.log("[" + label + "] Could not reach the Bag command; fighting on.", COLOR_ORANGE);
        return BattleHeal::NONE_LEFT;
    }
    pbf_press_button(context, BUTTON_A, 80ms, 2000ms);
    if (!find_bag_item_in_open_bag(stream, context, "potion", 1)){
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

//  How a battle is fought: every battle here takes the best move each turn.
struct BestMoveBattle{
    BattleType battle_type = BattleType::TRAINER;
    BattleFormat format = BattleFormat::SINGLES;
    //  After a faint the game shows the party: send out the next Pokemon.
    //  Without, a quiet two minutes is a timeout.
    bool switch_on_faint = false;
};

//  What a battle has done so far, across turns.
struct BattleProgress{
    bool battle_menu_seen = false;
    bool out_of_potions = false;
    size_t potions_used = 0;
    size_t turns = 0;
    size_t faint_switches = 0;
};

//  One turn at the battle menu: a Potion when a trainer's battle has left
//  our HP low, else the best move (and the default target in doubles).
void take_turn(
    VideoStream& stream, ProControllerContext& context,
    const BestMoveBattle& battle, BattleProgress& progress, const std::string& label
){
    progress.battle_menu_seen = true;
    // Early battles take a few turns; this many means the chosen
    // move is not being used (no PP, a misread menu).
    if (++progress.turns > 25){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": still at the battle menu after 25 turns.", stream);
    }
    stream.log("[AutoStory] Battle menu detected.", COLOR_BLUE);
    // A trainer battle we may lose: a Potion when HP is low.
    if (battle.battle_type == BattleType::TRAINER && !progress.out_of_potions && progress.potions_used < 4){
        const BattleHeal heal = battle_heal_if_low(stream, context, battle.battle_type, 0.35, label);
        if (heal == BattleHeal::USED){
            progress.potions_used++;
            return;
        }
        progress.out_of_potions = heal == BattleHeal::NONE_LEFT;
    }
    stream.log("[" + label + "] Move list position 1: use move 0.");
    use_best_move(stream, context, battle.battle_type, label);
    // A double battle asks for the target next; B would
    // back out of it. The default target is an opponent.
    if (battle.format == BattleFormat::DOUBLES){
        pbf_wait(context, 1200ms);
        pbf_press_button(context, BUTTON_A, 80ms, 1500ms);
    }
}

//  The battle has ended: its text to the overworld, which must stay
//  dialogue-free for four seconds.
void finish_battle(VideoStream& stream, ProControllerContext& context, const std::string& label){
    stream.log("[AutoStory] " + label + " battle complete.", COLOR_GREEN);
    OverworldWatcher overworld;
    const int cleanup_ret = run_until<ProControllerContext>(stream, context,
        [](ProControllerContext& ctx){
            pbf_mash_button(ctx, BUTTON_B, 120000ms);
        }, {{overworld}});
    if (cleanup_ret != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": Post-battle cleanup timed out.", stream);
    }
    context.wait_for_all_requests();
    confirm_battle_overworld(stream, context, label + " cleanup");
}

//  B through the battle's text until its menu, its end, or a move-learning
//  prompt shows: 0, 1 or 2; -1 after the wait.
int wait_for_battle_screen(
    VideoStream& stream, ProControllerContext& context, const BestMoveBattle& battle,
    const BattleProgress& progress, EndBattleWatcher& end_battle
){
    context.wait_for_all_requests();
    BattleMenuWatcher battle_menu(battle.battle_type);
    // Reuse the farming handlers' move-learning prompt recovery.
    SelectionArrowFinder learn_move(stream.overlay(), {0.50, 0.62, 0.40, 0.18}, COLOR_YELLOW);
    const bool watch_learn_move = battle.format == BattleFormat::DOUBLES || battle.switch_on_faint;
    return run_until<ProControllerContext>(
        stream,
        context,
        [&](ProControllerContext& context){
            // Clear narration with B while watching for commands.
            // A here can open Fight before selection begins.
            // With faint switching, a quiet 30 s means the party
            // screen after a faint, which B cannot leave.
            pbf_mash_button(context, BUTTON_B, battle.switch_on_faint ? 30000ms : 120000ms);
        },
        {
            {battle_menu},
            progress.battle_menu_seen ? PeriodicInferenceCallback{end_battle} : PeriodicInferenceCallback{},
            watch_learn_move ? PeriodicInferenceCallback{learn_move} : PeriodicInferenceCallback{},
        }
    );
}

//  Fights the battle on screen to its end and the overworld after it.
void fight_with_best_move(VideoStream& stream, ProControllerContext& context, const BestMoveBattle& battle, const std::string& label){
    stream.log("[AutoStory] Starting " + label + " battle...", COLOR_BLUE);
    // Preserve the existing early-game transition delay.
    pbf_wait(context, 5000ms);
    BattleProgress progress;
    EndBattleWatcher end_battle;
    while (true){
        const int ret = wait_for_battle_screen(stream, context, battle, progress, end_battle);
        if (ret == 0){
            take_turn(stream, context, battle, progress, label);
        }else if (ret == 1){
            finish_battle(stream, context, label);
            return;
        }else if (ret == 2){
            stream.log("[" + label + "] Declining move learning; preserving scripted move positions.");
            pbf_move_right_joystick(context, {0, -1}, 160ms, 840ms);
            pbf_press_button(context, BUTTON_ZL, 160ms, 840ms);
        }else if (battle.switch_on_faint && progress.faint_switches < 5){
            // After a faint the game shows the party: Down to the next
            // Pokemon, A, then A on "Send out" (by hand, 2026-10-29 against Roark).
            progress.faint_switches++;
            stream.log("[" + label + "] No battle menu for 30 s: a faint; sending out the next Pokemon.", COLOR_ORANGE);
            pbf_press_dpad(context, DPAD_DOWN, 80ms, 500ms);
            pbf_press_button(context, BUTTON_A, 80ms, 1200ms);
            pbf_press_button(context, BUTTON_A, 80ms, 2000ms);
        }else{
            stream.log("[AutoStory] " + label + " battle timeout.", COLOR_RED);
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                "[AutoStory] " + label + " battle timeout.", stream);
        }
    }
}

}

void confirm_battle_overworld(VideoStream& stream, ProControllerContext& context,
    const std::string& label){
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
    while (std::chrono::steady_clock::now() < deadline){
        const auto frame = stream.video().snapshot();
        if (!frame){
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": No video frame.", stream);
        }
        const auto now = std::chrono::steady_clock::now();
        if (dialogue.detect(frame) || black.detect(frame) || !overworld.detect(frame)){
            quiet_since = now;
        }else if (now - quiet_since >= 4s){
            finished = true;
            break;
        }
        pbf_mash_button(context, BUTTON_B, 100ms);
        context.wait_for_all_requests();
    }
    if (!finished){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            label + ": Dialogue-free overworld not confirmed for four seconds.", stream);
    }
    context.wait_for_all_requests();
}

void fight_story_battle(VideoStream& stream, ProControllerContext& context, const std::string& label, BattleFormat format){
    // The best move each turn; the next Pokemon after a faint; no new moves.
    fight_with_best_move(stream, context, {.battle_type = BattleType::TRAINER, .format = format, .switch_on_faint = true}, label);
}

void fight_trainer(VideoStream& stream, ProControllerContext& context, const std::string& label){
    // The best move on the menu each turn, whatever the starter.
    fight_with_best_move(stream, context, {.battle_type = BattleType::TRAINER, .format = BattleFormat::SINGLES, .switch_on_faint = false}, label);
}

void fight_wild_battle(VideoStream& stream, ProControllerContext& context, const std::string& label){
    fight_with_best_move(stream, context, {.battle_type = BattleType::STANDARD, .format = BattleFormat::SINGLES, .switch_on_faint = false}, label);
}


}
}
}
