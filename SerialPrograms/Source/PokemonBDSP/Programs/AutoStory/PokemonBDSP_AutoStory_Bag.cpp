/*  BDSP AutoStory - Bag
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Adapted from PokemonAutomation/Arduino-Source#1717 by Developer-Butters.
 *
 */

#include <cmath>
#include <cctype>
#include "CommonFramework/Exceptions/OperationFailedExceptionWithScreenshot.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/Async/InferenceRoutines.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "PokemonBDSP/Programs/AutoStory/Detect/PokemonBDSP_AutoStory_OverworldDetector.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_Actions.h"
#include "PokemonBDSP_AutoStory_Bag.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{

using namespace std::chrono_literals;


namespace{

ImageFloatBox bag_list_name_box(size_t row){
    // Previous height included the faint lower selection border in rows 2/6.
    // return ImageFloatBox(0.492, 0.213 + (row - 1) * 0.075, 0.335, 0.052);
    return ImageFloatBox(0.492, 0.213 + (row - 1) * 0.075, 0.335, 0.050);
}

int bag_confirm_pocket(VideoStream& stream, ProControllerContext& context){
    int previous = 0;
    size_t matches = 0;
    for (size_t check = 0; check < 10; check++){
        context.wait_for_all_requests();
        const auto frame = stream.video().snapshot();
        const int pocket = frame ? bag_pocket_position(frame) : 0;
        matches = pocket != 0 && pocket == previous ? matches + 1 : pocket != 0 ? 1 : 0;
        previous = pocket;
        if (matches >= 3){
            return pocket;
        }
        pbf_wait(context, 100ms);
    }
    return 0;
}

bool bag_navigate_to_pocket(VideoStream& stream, ProControllerContext& context, int target){
    int current = bag_confirm_pocket(stream, context);
    for (size_t attempt = 0; attempt < 3; attempt++){
        const auto plan = plan_bag_pocket_move(current, target);
        if (plan.action == BagPocketAction::CONFIRM){
            return true;
        }
        if (plan.action == BagPocketAction::UNKNOWN){
            stream.log("[Bag search] Selected pocket not detected.", COLOR_ORANGE);
            return false;
        }
        stream.log("[Bag search] Pocket " + std::to_string(current) + " to " + std::to_string(target) + ": " +
            (plan.action == BagPocketAction::RIGHT ? "right " : "left ") + std::to_string(plan.presses) +
            " presses, attempt " + std::to_string(attempt + 1) + "/3.");
        for (size_t press = 0; press < plan.presses; press++){
            pbf_press_dpad(context, plan.action == BagPocketAction::RIGHT ? DPAD_RIGHT : DPAD_LEFT, 80ms, 150ms);
        }
        pbf_wait(context, 350ms);
        current = bag_confirm_pocket(stream, context);
        stream.log(
            "[Bag search] Final pocket=" + std::to_string(current) + ", expected=" + std::to_string(target) + ".");
        if (current == target){
            return true;
        }
    }
    stream.log("[Bag search] Target pocket not reached after 3 movement batches.", COLOR_ORANGE);
    return false;
}

// Slot 1's colored HP bar in the Bag. Read fill position rather than OCR.
double potion_hp_fraction(const ImageViewRGB32& frame){
    if (bag_pocket_position(frame) == 0){
        return -1;
    }
    const ImageViewRGB32 bar = extract_box_reference(frame, ImageFloatBox{0.137, 0.160, 0.202, 0.016});
    size_t filled = 0;
    for (size_t x = 0; x < bar.width(); x++){
        size_t color = 0;
        for (size_t y = 0; y < bar.height(); y++){
            uint32_t p = bar.pixel(x, y);
            int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            const bool green = g > r + 30 && g > b + 20;
            const bool yellow = r > 150 && g > 120 && b < 100;
            const bool red = r > 150 && r > g + 50 && r > b + 50;
            color += green || yellow || red;
        }
        filled += color > bar.height() / 4;
    }
    return bar.width() ? (double)filled / bar.width() : -1;
}

double confirmed_potion_hp(VideoStream& stream, ProControllerContext& context){
    double previous = -1;
    size_t stable = 0;
    for (size_t check = 0; check < 20; check++){
        context.wait_for_all_requests();
        auto frame = stream.video().snapshot();
        double hp = frame ? potion_hp_fraction(frame) : -1;
        if (hp >= 0 && previous >= 0 && std::abs(hp - previous) <= 0.015){
            if (++stable >= 3){
                return hp;
            }
        }else{
            stable = 0;
        }
        previous = hp;
        pbf_wait(context, 150ms);
    }
    OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
        "Potion: Slot 1 HP bar could not be confirmed.",
        stream);
}

//  A walk down a pocket's list, reading the cursor's row.
struct BagScan{
    DpadPosition direction = DPAD_DOWN;
    std::string previous;
    size_t same = 0;
    size_t confirmed = 0;
    size_t unknown_cursor = 0;
};
enum class BagLook{ GO_ON, FOUND, NOT_FOUND };

//  The name on the cursor's row `cursor`, as lower-case letters.
std::string read_bag_row(const ImageViewRGB32& frame, int cursor){
    const std::string text = OCR::tesseract_ocr_read(Language::English,
        extract_box_reference(frame, bag_list_name_box(cursor)));
    std::string normalized;
    for (unsigned char ch : text){
        if (std::isalpha(ch)){
            normalized.push_back((char)std::tolower(ch));
        }
    }
    return normalized;
}

//  One look at the list: the row under the cursor is `name` (twice in a
//  row), or the list ran out both ways, or the cursor moves on.
BagLook bag_scan_step(VideoStream& stream, ProControllerContext& context, BagScan& scan, const std::string& name){
    context.wait_for_all_requests();
    const VideoSnapshot frame = stream.video().snapshot();
    if (!frame || bag_list_is_empty(*frame.frame)){
        stream.log("[Bag search] " + name + ": pocket is empty.", COLOR_ORANGE);
        return BagLook::NOT_FOUND;
    }
    const int cursor = bag_cursor_position(*frame.frame);
    if (cursor == 0){
        if (++scan.unknown_cursor >= 3){
            stream.log("[Bag search] " + name + ": cursor not found.", COLOR_ORANGE);
            return BagLook::NOT_FOUND;
        }
        pbf_wait(context, 200ms);
        return BagLook::GO_ON;
    }
    scan.unknown_cursor = 0;
    const std::string normalized = read_bag_row(*frame.frame, cursor);
    stream.log("[Bag search] Row " + std::to_string(cursor) + " reads \"" + normalized + "\".");
    if (normalized == name){
        if (++scan.confirmed >= 2){
            stream.log("[Bag search] " + name + " confirmed in row " + std::to_string(cursor) + ".", COLOR_GREEN);
            return BagLook::FOUND;
        }
        pbf_wait(context, 150ms);
        return BagLook::GO_ON;
    }
    scan.confirmed = 0;
    scan.same = normalized == scan.previous ? scan.same + 1 : 0;
    scan.previous = normalized;
    if (scan.same >= 2){
        if (scan.direction == DPAD_UP){
            stream.log("[Bag search] " + name + " not found.", COLOR_ORANGE);
            return BagLook::NOT_FOUND;
        }
        scan.direction = DPAD_UP;
        scan.same = 0;
    }
    pbf_press_dpad(context, scan.direction, 80ms, 300ms);
    return BagLook::GO_ON;
}

//  B out of the Bag to the overworld; fails with `failure` if it does not show.
void bag_back_to_overworld(VideoStream& stream, ProControllerContext& context, const std::string& failure){
    pbf_mash_button(context, BUTTON_B, 2000ms);
    context.wait_for_all_requests();
    OverworldWatcher overworld;
    if (wait_until(stream, context, std::chrono::seconds(10), {{overworld}}) != 0){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT, failure, stream);
    }
}

enum class Dose{ HEALED, GIVEN };

//  One Potion on the lead, from the overworld, its HP read before and after
//  in the Bag. HEALED when no dose is needed (HP full, or above
//  `heal_below` on the first look, or no Potion when only checking) or the
//  dose filled it; GIVEN when HP rose but is not full yet.
Dose potion_dose(VideoStream& stream, ProControllerContext& context, size_t dose, double heal_below){
    stream.log("[Potion] Finding Bag/Medicine/Potion; dose " + std::to_string(dose + 1) + "/3.");
    if (!find_bag_item_by_name(stream, context, "potion", 1, 5)){
        if (dose == 0 && heal_below < 1.0){
            // Only a check: without Potions there is nothing to do here.
            stream.log("[Potion] No Potion in the Bag; skipping the HP check.", COLOR_ORANGE);
            return Dose::HEALED;
        }
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Potion: Could not verify Potion in the Medicine pocket.",
            stream);
    }
    const double before = confirmed_potion_hp(stream, context);
    stream.log("[Potion] Slot 1 HP bar before use=" + std::to_string(before * 100) + "%.");
    if (before >= 0.98){
        return Dose::HEALED;
    }
    if (dose == 0 && before >= heal_below){
        stream.log("[Potion] Slot 1 HP is at least " + std::to_string(heal_below * 100) + "%; no Potion needed.");
        return Dose::HEALED;
    }
    if (before <= 0.005){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Potion: Slot 1 appears fainted; a Potion cannot revive it.",
            stream);
    }
    pbf_press_button(context, BUTTON_A, 80ms, 500ms);
    pbf_press_button(context, BUTTON_A, 80ms, 500ms);
    pbf_press_button(context, BUTTON_A, 80ms, 1000ms);
    bag_back_to_overworld(stream, context, "Potion: Did not return to the overworld after use.");
    // Reopen Bag to verify the result, including when the last Potion was used.
    open_menu(stream, context, MenuCursorPosition::BAG, 5);
    pbf_wait(context, 750ms);
    context.wait_for_all_requests();
    const double after = confirmed_potion_hp(stream, context);
    stream.log("[Potion] Slot 1 HP bar after use=" + std::to_string(after * 100) + "%.");
    if (after <= before + 0.005){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Potion: Slot 1 HP did not increase; refusing to continue movement.",
            stream);
    }
    return after >= 0.98 ? Dose::HEALED : Dose::GIVEN;
}

}

int bag_pocket_position(const ImageViewRGB32& frame){
    if (!frame){
        return 0;
    }
    int selected = 0;
    for (int pocket = 1; pocket <= 8; pocket++){
        // The lower left edge of each tab is below the red NEW badge.
        // This strip includes both the normal outline and enlarged selection.
        const auto edge =
            extract_box_reference(frame, ImageFloatBox(0.570 + (pocket - 1) * 0.0421, 0.098, 0.0045, 0.021));
        size_t colored = 0;
        for (size_t y = 0; y < edge.height(); y++){
            for (size_t x = 0; x < edge.width(); x++){
                const uint32_t pixel = edge.pixel(x, y);
                const int r = (pixel >> 16) & 255;
                const int g = (pixel >> 8) & 255;
                const int b = pixel & 255;
                // Red when focused, blue when the item list has focus.
                colored += (r > 160 && r > g + 60 && r > b + 60) || (b > 160 && b > r + 40 && b > g + 20);
            }
        }
        const double pixels = (double)edge.width() * edge.height();
        if (pixels > 0 && colored / pixels >= 0.18){
            if (selected != 0){
                return 0;
            }
            selected = pocket;
        }
    }
    return selected;
}

// One narrow strip on the left side of the red selection border. Divide its
// height into the seven row bands; the item's art and NEW badge are outside it.
int bag_cursor_position(const ImageViewRGB32& frame){
    if (!frame){
        return 0;
    }
    const ImageViewRGB32 strip = extract_box_reference(frame, ImageFloatBox(0.431, 0.229, 0.005, 0.470));
    double best = 0.0;
    double second = 0.0;
    int position = 0;
    for (int row = 0; row < 7; row++){
        const size_t top = static_cast<size_t>(strip.height() * (row * 0.075 / 0.470));
        const size_t bottom = static_cast<size_t>(strip.height() * ((row * 0.075 + 0.020) / 0.470));
        size_t red = 0;
        size_t total = 0;
        for (size_t y = top; y < bottom && y < strip.height(); y++){
            for (size_t x = 0; x < strip.width(); x++){
                const uint32_t pixel = strip.pixel(x, y);
                const unsigned r = (pixel >> 16) & 255;
                const unsigned g = (pixel >> 8) & 255;
                const unsigned b = pixel & 255;
                red += r > 180 && g < 120 && b < 120;
                total++;
            }
        }
        const double score = total == 0 ? 0.0 : static_cast<double>(red) / total;
        if (score > best){
            second = best;
            best = score;
            position = row + 1;
        }else if (score > second){
            second = score;
        }
    }
    return best >= 0.15 && best - second >= 0.10 ? position : 0;
}

bool bag_list_is_empty(const ImageViewRGB32& frame){
    if (!frame || bag_pocket_position(frame) == 0 || bag_cursor_position(frame) != 0){
        return false;
    }
    // Empty pockets have no item icons. The central Empty label is outside
    // this column; counting dark pixels in the name area included that label.
    const auto icons = extract_box_reference(frame, ImageFloatBox(0.449, 0.213, 0.032, 0.500));
    const auto stats = image_stats(icons);
    return stats.average.r > 220 && stats.average.g > 220 && stats.average.b > 220 && stats.stddev.sum() < 20;
}

BagPocketMovePlan plan_bag_pocket_move(int current, int target){
    if (current < 1 || current > 8 || target < 1 || target > 8){
        return {};
    }
    const int right = (target - current + 8) % 8;
    const int left = (current - target + 8) % 8;
    if (right == 0){
        return {BagPocketAction::CONFIRM, 0};
    }
    return right <= left ? BagPocketMovePlan{BagPocketAction::RIGHT, static_cast<size_t>(right)}
        : BagPocketMovePlan{BagPocketAction::LEFT, static_cast<size_t>(left)};
}

bool find_bag_item_by_name(
    VideoStream& stream,
    ProControllerContext& context,
    const std::string& name,
    int target_pocket,
    int num_icons
){
    open_menu(stream, context, MenuCursorPosition::BAG, num_icons);
    pbf_wait(context, 1000ms);
    return find_bag_item_in_open_bag(stream, context, name, target_pocket);
}

bool find_bag_item_in_open_bag(
    VideoStream& stream,
    ProControllerContext& context,
    const std::string& name,
    int target_pocket
){
    context.wait_for_all_requests();
    if (!bag_navigate_to_pocket(stream, context, target_pocket)){
        return false;
    }

    //  No reader for the game's language: the cursor's row, the pocket's
    //  first, if the pocket has one. Early in the story the pocket holds only
    //  what was bought or given (Potions in Medicine).
    if (!story_reads_text()){
        const VideoSnapshot frame = stream.video().snapshot();
        const bool any = frame && !bag_list_is_empty(*frame.frame) && bag_cursor_position(*frame.frame) != 0;
        stream.log("[Bag search] " + name + ": no text reader for the game's language; " +
            (any ? "taking the pocket's cursor row." : "the pocket is empty."), COLOR_ORANGE);
        return any;
    }

    //  Walk down the list reading the cursor row. The list does not wrap, so
    //  turn around once the cursor stops moving.
    BagScan scan;
    for (size_t look = 0; look < 128; look++){
        const BagLook result = bag_scan_step(stream, context, scan, name);
        if (result != BagLook::GO_ON){
            return result == BagLook::FOUND;
        }
    }
    stream.log("[Bag search] " + name + " not found.", COLOR_ORANGE);
    return false;
}

bool bag_has_item(VideoStream& stream, ProControllerContext& context, const std::string& name, int target_pocket){
    const bool found = find_bag_item_by_name(stream, context, name, target_pocket, 5);
    bag_back_to_overworld(stream, context, "Bag: Did not return to the overworld after looking for " + name + ".");
    return found;
}

void use_potion_first_pokemon(VideoStream& stream, ProControllerContext& context, double heal_below){
    OverlayBoxScope hp_overlay(stream.overlay(), COLOR_GREEN, {0.137, 0.160, 0.202, 0.016}, "");
    bool healed = false;
    for (size_t dose = 0; dose < 3 && !healed; dose++){
        healed = potion_dose(stream, context, dose, heal_below) == Dose::HEALED;
        if (!healed){
            pbf_mash_button(context, BUTTON_B, 2000ms);
            context.wait_for_all_requests();
        }
    }
    if (!healed){
        OperationFailedExceptionWithScreenshot::fire(ErrorReportMode::SEND_ERROR_REPORT,
            "Potion: Full HP not confirmed after three uses.",
            stream);
    }
    bag_back_to_overworld(stream, context, "Potion: Final overworld return not confirmed.");
    stream.log("[Potion] Slot 1 HP bar full and overworld confirmed.");
}



}
}
}
