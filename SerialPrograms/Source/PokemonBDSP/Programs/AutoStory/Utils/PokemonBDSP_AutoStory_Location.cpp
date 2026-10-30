/*  BDSP AutoStory - Location
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cctype>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonFramework/Language.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_RawTesseractOCR.h"
#include "PokemonBDSP_AutoStory_Location.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  The location banner: a white box at the top left that names a map for a
//  few seconds after the player enters it. Left of the centred name it is plain
//  white (251 on console B, against 27-75 for the trees there otherwise).
const ImageFloatBox LOCATION_BANNER_WHITE{0.035, 0.035, 0.085, 0.050};
const ImageFloatBox LOCATION_BANNER_TEXT{0.100, 0.025, 0.300, 0.080};

//  The save card's header: the map's name in salmon (255, 152, 140) on near
//  white (245), top left, right of the card's red border. Too faint for OCR
//  as it is: the reddish pixels are made black on white first.
const ImageFloatBox SAVE_SCREEN_LOCATION{0.036, 0.045, 0.274, 0.070};
const uint32_t SAVE_SCREEN_TEXT_MIN = 0xff960000;
const uint32_t SAVE_SCREEN_TEXT_MAX = 0xffffa9a9;

std::string digits_of(const std::string& text){
    std::string out;
    for (char ch : text){
        if (std::isdigit((unsigned char)ch)){
            out.push_back(ch);
        }
    }
    return out;
}

}


std::string normalize_location_name(const std::string& text){
    std::string out;
    for (unsigned char ch : text){
        if (std::isalnum(ch)){
            out.push_back((char)std::tolower(ch));
        }
    }
    return out;
}

std::string read_location_banner(const ImageViewRGB32& screen){
    const ImageStats white = image_stats(extract_box_reference(screen, LOCATION_BANNER_WHITE));
    if (white.average.sum() < 690 || white.stddev.sum() > 30){
        return "";
    }
    return normalize_location_name(OCR::tesseract_ocr_read(Language::English,
        extract_box_reference(screen, LOCATION_BANNER_TEXT)));
}

std::string read_save_screen_location(const ImageViewRGB32& screen){
    const ImageRGB32 text = to_blackwhite_rgb32_range(
        extract_box_reference(screen, SAVE_SCREEN_LOCATION), true, SAVE_SCREEN_TEXT_MIN, SAVE_SCREEN_TEXT_MAX);
    return normalize_location_name(OCR::tesseract_ocr_read(Language::English, text));
}

bool location_banner_matches(const std::string& read, const std::string& expected){
    const std::string a = normalize_location_name(read);
    const std::string b = normalize_location_name(expected);
    if (a.empty() || b.empty()){
        return false;
    }
    if (digits_of(a) != digits_of(b)){
        return false;
    }
    //  Levenshtein distance.
    std::vector<size_t> row(b.size() + 1);
    for (size_t j = 0; j <= b.size(); j++){
        row[j] = j;
    }
    for (size_t i = 1; i <= a.size(); i++){
        size_t diagonal = row[0];
        row[0] = i;
        for (size_t j = 1; j <= b.size(); j++){
            const size_t above = row[j];
            row[j] = std::min({row[j] + 1, row[j - 1] + 1, diagonal + (a[i - 1] == b[j - 1] ? 0 : 1)});
            diagonal = above;
        }
    }
    return row[b.size()] <= std::max<size_t>(1, b.size() / 4);
}


}
}
}
