/*  BDSP AutoStory - Move Selection
 *  Picks the best move on the battle's move menu, and selects a move slot
 *  with the cursor confirmed on screen.
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>

#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"

#include "PokemonBDSP_Autostory_Segment_01_tools.h"
#include "PokemonBDSP_AutoStory_MoveSelect.h"

using namespace std::chrono_literals;

namespace PokemonAutomation {
    namespace NintendoSwitch {
        namespace PokemonBDSP {

            // ---------------------------------------------------------------------------
            // Natalie battle
            // ---------------------------------------------------------------------------
            // Early moves the starters and their foes have, as type, power and
            // most PP. A status move has power 0.
            struct KnownMove { const char* name; const char* type; int power; int pp; };
            static const KnownMove KNOWN_MOVES[] = {
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
            static int read_move_pp(const ImageViewRGB32& screen, const ImageFloatBox& button, int max_pp) {
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
            static size_t pick_best_move(VideoStream& stream, const ImageViewRGB32& screen,
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
                    for (unsigned char ch : OCR::tesseract_ocr_read(Language::English, text)) {
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
                bool confirm_default_target, size_t current_move) {
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
                    BattleMenuOption selected = frame ? commands.selected_option(frame) : BattleMenuOption::UNKNOWN;
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

        }
    }
}
