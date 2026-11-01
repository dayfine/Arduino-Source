/*  BDSP AutoStory - Battle moves
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
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP_AutoStory_BattleMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

// Early moves the starters and their foes have, as type, power and
// most PP. A status move has power 0.
struct KnownMove{
    const char* name;
    const char* type;
    int power;
    int pp;
};
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

//  The box's text, whatever differs from its median colour by more than
//  120, as black on white with an 8-pixel margin.
ImageRGB32 pp_text_image(const ImageViewRGB32& box){
    std::vector<uint32_t> pixels;
    for (size_t y = 0; y < box.height(); y++){
        for (size_t x = 0; x < box.width(); x++){
            pixels.push_back(box.pixel(x, y));
        }
    }
    auto brightness = [](uint32_t p){ return ((p >> 16) & 255) + ((p >> 8) & 255) + (p & 255); };
    std::nth_element(pixels.begin(), pixels.begin() + pixels.size() / 2, pixels.end(),
        [&](uint32_t a, uint32_t b){ return brightness(a) < brightness(b); });
    const uint32_t background = pixels[pixels.size() / 2];
    const size_t pad = 8;
    ImageRGB32 text(box.width() + 2 * pad, box.height() + 2 * pad);
    text.fill(0xffffffff);
    for (size_t y = 0; y < box.height(); y++){
        for (size_t x = 0; x < box.width(); x++){
            const uint32_t p = box.pixel(x, y);
            int distance = 0;
            for (int shift : {16, 8, 0}){
                distance += std::abs((int)((p >> shift) & 255) - (int)((background >> shift) & 255));
            }
            if (distance > 120){
                text.pixel(x + pad, y + pad) = 0xff000000;
            }
        }
    }
    return text;
}

// A move button's PP left, "8/25" at its right end; -1 if it cannot be
// read. The text is dark, or yellow to red as PP runs low, so text is
// whatever differs from the button's own color. `max_pp` helps when
// the OCR reads the slash as a digit.
int read_move_pp(const ImageViewRGB32& screen, const ImageFloatBox& button, int max_pp){
    const ImageViewRGB32 box = extract_box_reference(screen, ImageFloatBox{
        button.x + button.width * 0.75, button.y + button.height * 0.10,
        button.width * 0.21, button.height * 0.80});
    if (box.width() == 0 || box.height() == 0){
        return -1;
    }
    const std::string read = OCR::tesseract_ocr_read(Language::English, pp_text_image(box));
    const size_t slash = read.find('/');
    std::string digits;
    for (size_t i = 0; i < read.size() && i < slash; i++){
        if (std::isdigit((unsigned char)read[i])){
            digits.push_back(read[i]);
        }
    }
    if (slash == std::string::npos && max_pp > 0){
        // "29135" for 29/35: drop the most PP and the misread slash.
        const std::string max = std::to_string(max_pp);
        if (digits.size() <= max.size() + 1 || digits.compare(digits.size() - max.size(), max.size(), max) != 0){
            return -1;
        }
        digits.resize(digits.size() - max.size() - 1);
    }
    if (digits.empty() || digits.size() > 2){
        return -1;
    }
    return std::stoi(digits);
}

//  A box's gray levels, row by row, and the cut between text and background:
//  halfway between the background and the darkest pixel (a fixed cut thins
//  the strokes on the blue button).
struct GrayBox{
    std::vector<int> gray;
    size_t width = 0;
    size_t height = 0;
    int threshold = 0;
    bool text_in_row(size_t row) const{
        for (size_t x = 0; x < width; x++){
            if (gray[row * width + x] < threshold){
                return true;
            }
        }
        return false;
    }
};
GrayBox gray_box(const ImageViewRGB32& box){
    GrayBox ret{std::vector<int>(box.width() * box.height()), box.width(), box.height(), 0};
    for (size_t y = 0; y < box.height(); y++){
        for (size_t x = 0; x < box.width(); x++){
            const uint32_t pixel = box.pixel(x, y);
            ret.gray[y * box.width() + x] = (int)(((pixel >> 16) & 0xff) + ((pixel >> 8) & 0xff) + (pixel & 0xff)) / 3;
        }
    }
    std::vector<int> sorted = ret.gray;
    std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
    const int background = sorted[sorted.size() / 2];
    const int darkest = *std::min_element(ret.gray.begin(), ret.gray.end());
    ret.threshold = (background + darkest) / 2;
    return ret;
}

//  The tallest band of text rows, [top, bottom): the name; "Effective" is smaller.
std::pair<size_t, size_t> tallest_text_band(const GrayBox& box){
    size_t band_top = 0, band_bottom = 0;
    for (size_t y = 0; y < box.height;){
        if (!box.text_in_row(y)){
            y++;
            continue;
        }
        size_t end = y;
        while (end < box.height && box.text_in_row(end)){
            end++;
        }
        if (end - y > band_bottom - band_top){
            band_top = y;
            band_bottom = end;
        }
        y = end;
    }
    return {band_top, band_bottom};
}

// The name is dark text on a light button (grey, or light blue under the
// cursor). Take the box right of the type icon and left of the PP, tall
// enough for a status move's name, which sits lower: it has no "Effective"
// line under it. Read as lower-case letters; nothing where there is no
// reader for the game's language (an unread move counts as a plain attack).
std::string read_move_name(const ImageViewRGB32& screen, const ImageFloatBox& button){
    const ImageFloatBox name_box{button.x + button.width * 0.17, button.y + button.height * 0.08,
        button.width * 0.53, button.height * 0.80};
    const GrayBox box = gray_box(extract_box_reference(screen, name_box));
    const auto [band_top, band_bottom] = tallest_text_band(box);
    // Black text on white, with a margin, for the OCR.
    const size_t pad = 6;
    const size_t top = band_top >= 4 ? band_top - 4 : 0;
    const size_t bottom = std::min(box.height, band_bottom + 4);
    ImageRGB32 text(box.width + 2 * pad, bottom - top + 2 * pad);
    text.fill(0xffffffff);
    for (size_t y = top; y < bottom; y++){
        for (size_t x = 0; x < box.width; x++){
            if (box.gray[y * box.width + x] < box.threshold){
                text.pixel(x + pad, y - top + pad) = 0xff000000;
            }
        }
    }
    std::string name;
    for (unsigned char ch : story_reads_text() ? OCR::tesseract_ocr_read(g_story_language, text) : std::string()){
        if (std::isalpha(ch)){
            name.push_back((char)std::tolower(ch));
        }
    }
    return name;
}

//  The longest known move whose name appears in `name`: "bubblebeam" over "bubble".
const KnownMove* known_move(const std::string& name){
    const KnownMove* known = nullptr;
    for (const KnownMove& move : KNOWN_MOVES){
        if (name.find(move.name) != std::string::npos &&
            (!known || std::string(move.name).size() > std::string(known->name).size())){
            known = &move;
        }
    }
    return known;
}

//  The move in slot `i`: its read name, and what it is if known. A slot
//  read well once keeps its name when a later read fails.
std::pair<std::string, const KnownMove*> identify_move(const ImageViewRGB32& screen, const ImageFloatBox& button, size_t i){
    static std::array<std::string, 4> last_read;
    std::string name = read_move_name(screen, button);
    const KnownMove* known = known_move(name);
    if (!known){
        name = last_read[i];
        known = known_move(name);
    }
    if (known){
        last_read[i] = known->name;
    }
    return {name, known};
}

}


size_t pick_best_move(VideoStream& stream, const ImageViewRGB32& screen,
    const BattleMoveMenuState& moves, const std::string& label){
    // Keep the menu as it looked, to check the reads against.
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/moves/";
    Filesystem::create_directories(Filesystem::Path(folder));
    screen.save(folder + now_to_filestring() + "-" + label + ".png");
    size_t best = 1;
    int best_score = -1;
    bool any_attack = false;
    size_t last_not_status = 1;
    for (size_t i = 0; i < moves.move_count; i++){
        const auto [name, known] = identify_move(screen, moves.buttons[i], i);
        // A move with no PP left cannot be chosen; the game says so and
        // keeps the menu up.
        const int pp = read_move_pp(screen, moves.buttons[i], known ? known->pp : 0);
        stream.log("[" + label + "] Move " + std::to_string(i + 1) + " PP left: " + (pp < 0 ? std::string("unread") : std::to_string(pp)) + ".");
        if (pp == 0){
            continue;
        }
        // An unreadable move counts as a plain attack, ahead of known
        // status moves.
        const int score = known ? known->power * 2 + (std::string(known->type) != "normal" ? 1 : 0) : 40 * 2 - 1;
        stream.log("[" + label + "] Move " + std::to_string(i + 1) + " reads \"" + name + "\"" +
            (known ? std::string(" = ") + known->name + " (" + known->type + ", " + std::to_string(known->power) + ")" : " (unknown)") + ".");
        if (score > best_score){
            best_score = score;
            best = i + 1;
        }
        any_attack = any_attack || (known && known->power > 0);
        if (!known || known->power > 0){
            last_not_status = i + 1;
        }
    }
    // No attack read: the newest move is the starter's own-type attack
    // (Water Gun, Ember, Absorb), unless it is a status move.
    if (!any_attack){
        best = last_not_status;
    }
    stream.log("[" + label + "] Best move: " + std::to_string(best) + ".");
    return best;
}



}
}
}
