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
#include <string_view>
#include <vector>
#include "Common/Cpp/Filesystem/Filesystem.h"
#include "Common/Cpp/Json/JsonObject.h"
#include "Common/Cpp/Json/JsonValue.h"
#include "Common/Cpp/PrettyPrint.h"
#include "CommonFramework/GlobalAutoPaths.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "CommonTools/OCR/OCR_SmallDictionaryMatcher.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Location.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_StoryLanguage.h"
#include "PokemonBDSP_AutoStory_BattleMoves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  How close a read must come to a move's name, as the other move readers.
constexpr double MOVE_NAME_MAX_LOG10P = -1.30;
constexpr double MOVE_NAME_LOG10P_SPREAD = 0.50;

//  Early moves the starters and their foes have, by the slug of the move
//  name dictionary, as type, power and most PP. A status move has power 0.
struct KnownMove{
    std::string slug;
    std::string type;
    int power;
    int pp;
};
const KnownMove KNOWN_MOVES[] = {
    {"pound", "normal", 40, 35}, {"tackle", "normal", 40, 35}, {"scratch", "normal", 40, 35},
    {"quick-attack", "normal", 40, 30}, {"headbutt", "normal", 70, 15},
    {"water-gun", "water", 40, 25}, {"bubble", "water", 40, 30}, {"bubble-beam", "water", 65, 20},
    {"ember", "fire", 40, 25}, {"flame-wheel", "fire", 60, 25}, {"mach-punch", "fighting", 40, 30},
    {"absorb", "grass", 20, 25}, {"razor-leaf", "grass", 55, 25}, {"bite", "dark", 60, 25},
    {"peck", "flying", 35, 35}, {"pluck", "flying", 60, 20}, {"wing-attack", "flying", 60, 35},
    {"metal-claw", "steel", 50, 35},
    {"growl", "normal", 0, 40}, {"leer", "normal", 0, 30}, {"tail-whip", "normal", 0, 30},
    {"withdraw", "water", 0, 40}, {"taunt", "dark", 0, 20}, {"charm", "fairy", 0, 20},
    {"curse", "ghost", 0, 10},
};

const KnownMove* known_move(std::string_view slug){
    for (const KnownMove& move : KNOWN_MOVES){
        if (slug == move.slug){
            return &move;
        }
    }
    return nullptr;
}

//  What each starter has in each slot as BDSP teaches it, learnt in this
//  order up to level 13: slot 1 is a Normal attack, slot 2 a status move,
//  slot 3 the starter's own-type attack, slot 4 either.
const std::vector<std::vector<std::string_view>> STARTER_SLOTS = {
    {"tackle", "scratch", "pound"},
    {"withdraw", "leer", "growl"},
    {"absorb", "ember", "water-gun"},
    {"razor-leaf", "taunt", "charm"},
};

//  A slot whose name was not read, from its PP, which rules out any move
//  with less most PP, among what the starters have in that slot (any known
//  move if none of those is left): a known move when every move left is an
//  attack (the weakest) or every one a status move.
const KnownMove* guess_unread_move(size_t slot, int pp){
    std::vector<const KnownMove*> left;
    if (slot < STARTER_SLOTS.size()){
        for (std::string_view slug : STARTER_SLOTS[slot]){
            left.push_back(known_move(slug));
        }
    }
    auto pp_rules_out = [&](const KnownMove* move){ return pp > move->pp; };
    std::erase_if(left, pp_rules_out);
    if (left.empty()){
        for (const KnownMove& move : KNOWN_MOVES){
            left.push_back(&move);
        }
        std::erase_if(left, pp_rules_out);
    }
    if (left.empty()){
        return nullptr;
    }
    const KnownMove* weakest = left[0];
    for (const KnownMove* move : left){
        if ((move->power > 0) != (weakest->power > 0)){
            return nullptr;
        }
        if (move->power < weakest->power){
            weakest = move;
        }
    }
    return weakest;
}

//  Every language's names of the known moves, from the move name dictionary.
JsonObject known_move_names(){
    const JsonValue all = load_json_file(RESOURCE_PATH() + "PokemonSV/PokemonMovesOCR.json");
    JsonObject known;
    for (const auto& [language, names] : all.to_object_throw()){
        JsonObject names_known;
        for (const KnownMove& move : KNOWN_MOVES){
            const JsonValue* name = names.to_object_throw().get_value(move.slug);
            if (name){
                names_known[move.slug] = name->clone();
            }
        }
        known[language] = std::move(names_known);
    }
    return known;
}

const OCR::SmallDictionaryMatcher& move_name_matcher(){
    static const OCR::SmallDictionaryMatcher matcher(known_move_names());
    return matcher;
}

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

//  The name is dark text on a light button (grey, or light blue under the
//  cursor). Take the box right of the type icon and left of the PP, tall
//  enough for a status move's name, which sits lower: it has no "Effective"
//  line under it. Returns the slug of the known move it reads as, in the
//  story's language; "" if none.
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
    const OCR::StringMatchResult read = move_name_matcher().match_substring_from_image(
        nullptr, story_language(), text, MOVE_NAME_MAX_LOG10P, MOVE_NAME_LOG10P_SPREAD);
    if (read.results.size() != 1){
        return "";
    }
    return read.results.begin()->second.token;
}

//  `slug` read in slot `i`, or what the slot read last time if this read
//  failed.
std::string remember_move(std::string slug, size_t i){
    static std::array<std::string, 4> last_read;
    if (slug.empty()){
        return last_read[i];
    }
    last_read[i] = slug;
    return slug;
}

} // namespace


std::string MoveSlot::describe(size_t slot) const{
    const KnownMove* known = known_move(slug);
    const KnownMove* guess = known ? nullptr : guess_unread_move(slot, pp);
    const std::string what = known
        ? std::string(known->slug) + " (" + known->type + ", " + std::to_string(known->power) + ")"
        : guess
            ? std::string("unread, ") + (guess->power > 0 ? "an attack" : "a status move") + " by slot and PP"
            : "unread";
    return what + ", PP left " + (pp < 0 ? std::string("unread") : std::to_string(pp));
}

size_t choose_move(const std::vector<MoveSlot>& slots){
    size_t best = 0;
    int best_score = -1;
    for (size_t i = 0; i < slots.size(); i++){
        // A move with no PP left cannot be chosen; the game says so and
        // keeps the menu up.
        if (slots[i].pp == 0){
            continue;
        }
        const KnownMove* known = known_move(slots[i].slug);
        if (!known){
            known = guess_unread_move(i, slots[i].pp);
        }
        // A move that might be either counts below any attack known for
        // sure, and an earlier one first: slot 1 is an attack for every
        // starter.
        const int score = known ? known->power * 2 + (std::string_view(known->type) != "normal" ? 1 : 0) : 1;
        if (score > best_score){
            best_score = score;
            best = i;
        }
    }
    return best + 1;
}

std::vector<MoveSlot> read_move_menu(const ImageViewRGB32& screen, const BattleMoveMenuState& moves){
    std::vector<MoveSlot> slots;
    for (size_t i = 0; i < moves.move_count; i++){
        MoveSlot slot;
        slot.slug = read_move_name(screen, moves.buttons[i]);
        const KnownMove* known = known_move(slot.slug);
        slot.pp = read_move_pp(screen, moves.buttons[i], known ? known->pp : 0);
        slots.push_back(std::move(slot));
    }
    return slots;
}

size_t pick_best_move(VideoStream& stream, const ImageViewRGB32& screen,
    const BattleMoveMenuState& moves, const std::string& label){
    // Keep the menu as it looked, to check the reads against.
    const std::string folder = SCREENSHOTS_PATH() + "AutoStory/moves/";
    Filesystem::create_directories(Filesystem::Path(folder));
    screen.save(folder + now_to_filestring() + "-" + label + ".png");
    std::vector<MoveSlot> slots = read_move_menu(screen, moves);
    for (size_t i = 0; i < slots.size(); i++){
        slots[i].slug = remember_move(std::move(slots[i].slug), i);
        stream.log("[" + label + "] Move " + std::to_string(i + 1) + ": " + slots[i].describe(i) + ".");
    }
    const size_t best = choose_move(slots);
    stream.log("[" + label + "] Best move: " + std::to_string(best) + ".");
    return best;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
