/*  BDSP AutoStory - Name Limit Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/Language.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonBDSP_AutoStory_NameLimitReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


namespace{

//  The counter, with room for "12/12" to its left.
const ImageFloatBox KEYBOARD_COUNTER_BOX(0.62, 0.32, 0.085, 0.05);

//  The number at `text[pos]`, and where it ends; nullopt when no digit is there.
std::optional<size_t> read_digits(std::string_view text, size_t& pos){
    const size_t start = pos;
    size_t value = 0;
    while (pos < text.size() && text[pos] >= '0' && text[pos] <= '9' && pos - start < 4){
        value = value * 10 + (size_t)(text[pos] - '0');
        pos++;
    }
    if (pos == start){
        return std::nullopt;
    }
    return value;
}

void skip_spaces(std::string_view text, size_t& pos){
    while (pos < text.size() && (text[pos] == ' ' || text[pos] == '\n' || text[pos] == '\r')){
        pos++;
    }
}

} // namespace


std::optional<size_t> parse_keyboard_counter(std::string_view text){
    size_t pos = 0;
    skip_spaces(text, pos);
    if (!read_digits(text, pos)){
        return std::nullopt;
    }
    skip_spaces(text, pos);
    if (pos >= text.size() || text[pos] != '/'){
        return std::nullopt;
    }
    pos++;
    skip_spaces(text, pos);
    const std::optional<size_t> limit = read_digits(text, pos);
    skip_spaces(text, pos);
    if (!limit || *limit == 0 || pos != text.size()){
        return std::nullopt;
    }
    return limit;
}

std::optional<size_t> read_keyboard_name_limit(const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return std::nullopt;
    }
    const std::string text = OCR::ocr_read(Language::English, extract_box_reference(screen, KEYBOARD_COUNTER_BOX));
    return parse_keyboard_counter(text);
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
