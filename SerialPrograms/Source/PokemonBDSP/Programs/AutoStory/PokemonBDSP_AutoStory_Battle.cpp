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

// Early moves the starters and their foes have, as type, power and
// most PP. A status move has power 0.
struct KnownMove { const char* name; const char* type; int power; int pp; };
const KnownMove KNOWN_MOVES[] = {
    {"pound", "normal", 40, 35}, {"tackle", "normal", 40, 35}, {"scratch", "normal", 40, 35},
    {"quickattack", "normal", 40, 30}, {"headbutt", "normal", 70, 15},
    {"watergun", "water", 40, 25}, {"bubble", "water", 40, 30}, {"bubblebeam", "water", 65, 20},
    {"ember", "fire", 40, 25}, {"flamewheel", "fire", 60, 25}, {"machpunch", "fighting", 40, 30},
    {"absorb", "grass", 20, 25}, {"razorleaf", "grass", 55, 25}, {"bite", "dark", 60, 25},
    {"peck", "flying", 35, 35}, {"wingattack", "flying", 60, 35}, {"metalclaw", "steel", 50, 35},
    {"growl", "normal", 0, 40}, {"leer", "normal", 0, 30}, {"tailwhip", "normal", 0, 30},
    {"withdraw", "water", 0, 40}, {"taunt", "dark", 0, 20},
};

// A move button's PP left, "8/25" at its right end; -1 if it cannot be
// read. The text is dark, or yellow to red as PP runs low, so text is
// whatever differs from the button's own color. `max_pp` helps when
// the OCR reads the slash as a digit.
int read_move_pp(const ImageViewRGB32& screen, const ImageFloatBox& button, int max_pp) {
    const ImageViewRGB32 box = extract_box_reference(screen, ImageFloatBox{
        button.x + button.width * 0.75, button.y + button.height * 0.10,
        button.width * 0.21, button.height * 0.80});
    if (box.width() == 0 || box.height() == 0) return -1;
    std::vector<uint32_t> pixels;
    for (size_t y = 0; y < box.height(); y++) for (size_t x = 0; x < box.width(); x++) pixels.push_back(box.pixel(x, y));
    auto brightness = [](uint32_t p) { return ((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255); };
    std::vector<uint32_t> sorted = pixels;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end(),
        [&](uint32_t a, uint32_t b) { return brightness(a) < brightness(b); });
    const uint32_t background = sorted[sorted.size() / 2];
    const size_t pad = 8;
    ImageRGB32 text(box.width() + 2 * pad, box.height() + 2 * pad);
    text.fill(0xffffffff);
    for (size_t y = 0; y < box.height(); y++) {
        for (size_t x = 0; x < box.width(); x++) {
            const uint32_t p = box.pixel(x, y);
            int distance = 0;
            for (int shift : {16, 8, 0}) distance += std::abs((int)((p >> shift) & 255) - (int)((background >> shift) & 255));
            if (distance > 120) text.pixel(x + pad, y + pad) = 0xff000000;
        }
    }
    const std::string read = OCR::tesseract_ocr_read(Language::English, text);
    const size_t slash = read.find('/');
    std::string digits;
    for (size_t i = 0; i < read.size() && i < slash; i++) if (std::isdigit((unsigned char)read[i])) digits.push_back(read[i]);
    if (slash == std::string::npos && max_pp > 0) {
        // "29135" for 29/35: drop the most PP and the misread slash.
        const std::string max = std::to_string(max_pp);
        if (digits.size() > max.size() + 1 && digits.compare(digits.size() - max.size(), max.size(), max) == 0) {
            digits.resize(digits.size() - max.size() - 1);
        } else {
            return -1;
        }
    }
    if (digits.empty() || digits.size() > 2) return -1;
    return std::stoi(digits);
}

// The best move on the move menu: the most powerful one we know, and
// on a tie one that is not Normal (an early starter's own type).
// Reads the names with OCR; 1 if none is known.
size_t pick_best_move(VideoStream& stream, const ImageViewRGB32& screen,
    const BattleMoveMenuState& moves, const std::string& label) {
    // Keep the menu as it looked, to check the reads against.
    {
        const std::string folder = SCREENSHOTS_PATH() + "AutoStory/moves/";
        Filesystem::create_directories(Filesystem::Path(folder));
        screen.save(folder + now_to_filestring() + "-" + label + ".png");
    }
    size_t best = 1;
    int best_score = -1;
    bool any_attack = false;
    size_t last_not_status = 1;
    for (size_t i = 0; i < moves.move_count; i++) {
        // The name is dark text on a light button (grey, or light blue under the
        // cursor). Take the box right of the type icon and left of the PP, tall
        // enough for a status move's name, which sits lower: it has no "Effective"
        // line under it.
        const ImageFloatBox& button = moves.buttons[i];
        const ImageFloatBox name_box{button.x + button.width * 0.17, button.y + button.height * 0.08,
            button.width * 0.53, button.height * 0.80};
        const ImageViewRGB32 box = extract_box_reference(screen, name_box);
        // Text is darker than halfway between the background and the darkest pixel;
        // a fixed cut thins the strokes on the blue button.
        std::vector<int> gray(box.width() * box.height());
        for (size_t y = 0; y < box.height(); y++) {
            for (size_t x = 0; x < box.width(); x++) {
                const uint32_t pixel = box.pixel(x, y);
                gray[y * box.width() + x] = (int)(((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff)) / 3;
            }
        }
        std::vector<int> sorted = gray;
        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
        const int background = sorted[sorted.size() / 2];
        const int darkest = *std::min_element(gray.begin(), gray.end());
        const int threshold = (background + darkest) / 2;
        // The name is the tallest band of text rows; "Effective" is smaller.
        size_t band_top = 0, band_bottom = 0;
        for (size_t y = 0; y < box.height();) {
            auto has_text = [&](size_t row) {
                for (size_t x = 0; x < box.width(); x++) if (gray[row * box.width() + x] < threshold) return true;
                return false;
            };
            if (!has_text(y)) { y++; continue; }
            size_t end = y;
            while (end < box.height() && has_text(end)) end++;
            if (end - y > band_bottom - band_top) { band_top = y; band_bottom = end; }
            y = end;
        }
        // Black text on white, with a margin, for the OCR.
        const size_t pad = 6;
        const size_t top = band_top >= 4 ? band_top - 4 : 0;
        const size_t bottom = std::min(box.height(), band_bottom + 4);
        ImageRGB32 text(box.width() + 2 * pad, bottom - top + 2 * pad);
        text.fill(0xffffffff);
        for (size_t y = top; y < bottom; y++) {
            for (size_t x = 0; x < box.width(); x++) {
                if (gray[y * box.width() + x] < threshold) text.pixel(x + pad, y - top + pad) = 0xff000000;
            }
        }
        std::string name;
        // Move names only where there is a reader for the game's
        // language; an unread move counts as a plain attack.
        for (unsigned char ch : story_reads_text() ? OCR::tesseract_ocr_read(g_story_language, text) : std::string()) {
            if (std::isalpha(ch)) name.push_back((char)std::tolower(ch));
        }
        // A slot read well once keeps its name when a later read fails.
        static std::array<std::string, 4> last_read;
        const KnownMove* known = nullptr;
        for (int pass = 0; pass < 2 && !known; pass++) {
        if (pass == 1) name = last_read[i];
        for (const KnownMove& move : KNOWN_MOVES) {
            //  The longest name that appears wins: "bubblebeam" over "bubble".
            if (name.find(move.name) != std::string::npos &&
                (!known || std::string(move.name).size() > std::string(known->name).size())) {
                known = &move;
            }
        }
        }
        if (known) last_read[i] = known->name;
        // A move with no PP left cannot be chosen; the game says so and
        // keeps the menu up.
        const int pp = read_move_pp(screen, button, known ? known->pp : 0);
        stream.log("[" + label + "] Move " + std::to_string(i + 1) + " PP left: " + (pp < 0 ? std::string("unread") : std::to_string(pp)) + ".");
        if (pp == 0) {
            continue;
        }
        // An unreadable move counts as a plain attack, ahead of known
        // status moves.
        const int score = known ? known->power * 2 + (std::string(known->type) != "normal" ? 1 : 0) : 40 * 2 - 1;
        stream.log("[" + label + "] Move " + std::to_string(i + 1) + " reads \"" + name + "\"" +
            (known ? std::string(" = ") + known->name + " (" + known->type + ", " + std::to_string(known->power) + ")" : " (unknown)") + ".");
        if (score > best_score) { best_score = score; best = i + 1; }
        if (known && known->power > 0) any_attack = true;
        if (!known || known->power > 0) last_not_status = i + 1;
    }
    // No attack read: the newest move is the starter's own-type attack
    // (Water Gun, Ember, Absorb), unless it is a status move.
    if (!any_attack) best = last_not_status;
    stream.log("[" + label + "] Best move: " + std::to_string(best) + ".");
    return best;
}

void select_battle_move_slot(VideoStream& stream, ProControllerContext& context,
    BattleType battle_type, size_t target, const std::string& label,
    bool confirm_default_target = false, size_t current_move = 1) {
    BattleMenuDetector commands(battle_type);
    VideoOverlaySet overlays(stream.overlay());
    commands.make_overlays(overlays);
    overlays.add(COLOR_GREEN, {0.90, 0.48, 0.075, 0.505});
    // Reenter Fight from an already-open list without assuming
    // that this resets the remembered move cursor.
    if (confirm_default_target) {
        context.wait_for_all_requests();
        const auto frame=stream.video().snapshot();
        if (frame && detect_battle_move_menu(frame).move_count) {
            pbf_press_button(context,BUTTON_B,80ms,500ms);
        }
    }
    bool fight = false;
    size_t confirmed = 0;
    for (size_t check = 0; check < 30; check++) {
        context.wait_for_all_requests();
        auto frame = stream.video().snapshot();
        BattleMenuOption selected = frame ? battle_menu_selected_option(commands, frame) : BattleMenuOption::UNKNOWN;
        stream.log("[" + label + "] Command cursor=" + std::to_string((int)selected + 1) + "; target Battle=1.");
        if (selected == BattleMenuOption::BATTLE) {
            if (++confirmed >= 3) { fight = true; break; }
            pbf_wait(context, 100ms);
        } else {
            confirmed = 0;
            if (selected == BattleMenuOption::UNKNOWN) pbf_wait(context, 150ms);
            else pbf_press_dpad(context, DPAD_UP, 80ms, 250ms);
        }
    }
    if (!fight) OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": Could not confirm Battle command; A not pressed.", stream);
    pbf_press_button(context,BUTTON_A,80ms,confirm_default_target ? 1000ms : 500ms);
    if (confirm_default_target) {
        // Reuse the farming move/target confirmation sequence, but
        // navigate relative to this battle's retained move cursor.
        // Reopening Fight does not reset it to move 1.
        size_t available=0,stable=0;
        for (size_t check=0;check<20 && stable<3;check++) {
            context.wait_for_all_requests();
            const auto frame=stream.video().snapshot();
            const auto moves=frame ? detect_battle_move_menu(frame) : BattleMoveMenuState{};
            if (moves.move_count && moves.move_count==available) stable++;
            else {available=moves.move_count;stable=moves.move_count ? 1:0;}
            if (stable<3) pbf_wait(context,100ms);
        }
        if (stable<3 || available<target || available<current_move) OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,label+": Requested move position is not available.",stream);
        stream.log("["+label+"] Retained move cursor "+std::to_string(current_move)+
            " -> "+std::to_string(target)+".");
        for (size_t slot=current_move;slot<target;slot++) pbf_press_dpad(context,DPAD_DOWN,80ms,400ms);
        for (size_t slot=current_move;slot>target;slot--) pbf_press_dpad(context,DPAD_UP,80ms,400ms);
        pbf_press_button(context,BUTTON_A,80ms,1000ms);
        pbf_press_button(context,BUTTON_A,80ms,3000ms);
        return;
    }
    confirmed = 0;
    size_t previous_move_count = 0, stable_move_count = 0;
    for (size_t check = 0; check < 30; check++) {
        context.wait_for_all_requests();
        auto frame = stream.video().snapshot();
        const BattleMoveMenuState moves = frame ? detect_battle_move_menu(frame) : BattleMoveMenuState{};
        const int slot = (int)moves.selected_slot;
        VideoOverlaySet move_overlays(stream.overlay());
        for (size_t i = 0; i < moves.move_count; i++)
            move_overlays.add(moves.selected_slot == i + 1 ? COLOR_GREEN : COLOR_YELLOW, moves.buttons[i]);
        stream.log("[" + label + "] Available moves=" + std::to_string(moves.move_count) +
            "; selected slot=" + std::to_string(slot) + "/4; target=" + std::to_string(target) + ".");
        if (moves.move_count == previous_move_count && moves.move_count > 0) stable_move_count++;
        else { previous_move_count = moves.move_count; stable_move_count = 1; confirmed = 0; }
        // Target 0: pick the best move once the menu has settled.
        if (target == 0 && stable_move_count >= 3 && frame) {
            target = pick_best_move(stream, *frame.frame, moves, label);
        }
        if (moves.move_count > 0 && moves.move_count < target && stable_move_count >= 3) {
            OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
                label + ": Move slot " + std::to_string(target) + " is absent; no move confirmed.", stream);
        }
        if (slot == (int)target && stable_move_count >= 3) {
            if (++confirmed >= 3) {
                stream.log("[" + label + "] Move " + std::to_string(target) + " confirmed; using it.");
                pbf_press_button(context, BUTTON_A, 80ms, 3000ms);
                return;
            }
            pbf_wait(context, 100ms);
        } else {
            confirmed = 0;
            if (slot == 0 || slot == (int)target) pbf_wait(context, 150ms);
            else pbf_press_dpad(context, slot < (int)target ? DPAD_DOWN : DPAD_UP, 80ms, 250ms);
        }
        context.wait_for_all_requests();
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        label + ": Could not confirm move " + std::to_string(target) + "; A not pressed.", stream);
}

// Our HP bar in the battle's bottom-left panel, as a fraction; -1 if
// no bar is there. Measured on 1920x1080 frames: the bar runs from
// x 88 to 466 on rows 961 to 965.
double battle_player_hp(const ImageViewRGB32& frame) {
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
BattleHeal battle_heal_if_low(VideoStream& stream, ProControllerContext& context,
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
        const BattleMenuOption selected = frame ? battle_menu_selected_option(commands, frame) : BattleMenuOption::UNKNOWN;
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

void fight_using_move_list(
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

            // Battle menu detected.
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

            // End battle detected.
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

void fight_story_battle(VideoStream& stream, ProControllerContext& context, const std::string& label, bool doubles) {
    // The best move each turn; the next Pokemon after a faint; no new moves.
    fight_using_move_list(stream, context, BattleType::TRAINER, {0}, label, false, true, doubles, true);
}

void fight_trainer(VideoStream& stream, ProControllerContext& context, const std::string& label) {
    // Move 0: the best move on the menu each turn, whatever the starter.
    fight_using_move_slot(stream, context, BattleType::TRAINER, 0, label);
}

void fight_wild_battle(VideoStream& stream, ProControllerContext& context, const std::string& label){
    fight_using_move_slot(stream, context, BattleType::STANDARD, 0, label);
}


}
}
}
