/*  PokemonLZA AutoStory - Backup card reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include <format>
#include <string>
#include <string_view>
#include "CommonFramework/Language.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonLZA_AutoStoryBackupCardReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The card's plain white strips at 1080p: down its right and left edges and under its
//  button; none next to the title, whose width varies by language.
const ImageFloatBox CARD_WHITE_STRIPS[] = {
    {1672.0 / 1920, 432.0 / 1080, 23.0 / 1920, 324.0 / 1080},
    {230.0 / 1920, 432.0 / 1080, 23.0 / 1920, 324.0 / 1080},
    {288.0 / 1920, 778.0 / 1080, 1344.0 / 1920, 43.0 / 1080},
};

//  The "Registered in the Pokedex" count, under its label.
const ImageFloatBox REGISTERED_BOX(700.0 / 1920, 510.0 / 1080, 160.0 / 1920, 50.0 / 1080);

bool white_strip(const ImageViewRGB32& frame, const ImageFloatBox& box){
    const ImageStats stats = image_stats(extract_box_reference(frame, box));
    return stats.average.sum() >= 3 * 235 && stats.stddev.sum() <= 30;
}

bool backup_card_shown(const ImageViewRGB32& frame){
    if (!frame){
        return false;
    }
    for (const ImageFloatBox& box : CARD_WHITE_STRIPS){
        if (!white_strip(frame, box)){
            return false;
        }
    }
    return true;
}

} // namespace


std::optional<size_t> read_backup_card_registered(Logger& logger, const ImageViewRGB32& frame){
    if (!backup_card_shown(frame)){
        return std::nullopt;
    }
    //  Only the leading digits: read_number() maps the Japanese counter kanji ("3a") to 34.
    const std::string text = OCR::ocr_read(Language::English, extract_box_reference(frame, REGISTERED_BOX));
    std::string_view rest(text);
    while (!rest.empty() && rest.front() == ' '){
        rest.remove_prefix(1);
    }
    size_t registered = 0;
    size_t digits = 0;
    while (digits < rest.size() && rest[digits] >= '0' && rest[digits] <= '9'){
        registered = registered * 10 + (size_t)(rest[digits] - '0');
        digits++;
    }
    logger.log(std::format("[Rewind] backup card count: OCR \"{}\" -> {}", text.substr(0, text.find('\n')), digits == 0 ? std::string("no digits") : std::to_string(registered)));
    if (digits == 0){
        return std::nullopt;
    }
    return registered;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
