/*  PokemonLZA AutoStory - Backup card reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "PokemonLZA_AutoStoryBackupCardReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  The card's plain white strips at 1920 x 1080: right of the "BACKUP"
//  title, down its left edge, and under its button.
const ImageFloatBox CARD_WHITE_STRIPS[] = {
    {576.0 / 1920, 173.0 / 1080, 960.0 / 1920, 54.0 / 1080},
    {230.0 / 1920, 432.0 / 1080, 23.0 / 1920, 324.0 / 1080},
    {288.0 / 1920, 778.0 / 1080, 1344.0 / 1920, 43.0 / 1080},
};

//  The "Registered in the Pokedex" count, under its label in the card's
//  lower panel: x 700 to 860, y 510 to 560 at 1920 x 1080.
const ImageFloatBox REGISTERED_BOX(700.0 / 1920, 510.0 / 1080, 160.0 / 1920, 50.0 / 1080);

//  White: each channel near 255, and flat.
bool white_strip(const ImageViewRGB32& frame, const ImageFloatBox& box){
    const ImageStats stats = image_stats(extract_box_reference(frame, box));
    return stats.average.sum() >= 3 * 235 && stats.stddev.sum() <= 30;
}

//  Whether `frame` shows the backup card: white at its top, left edge and
//  bottom, around its panels.
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
    const int registered = OCR::read_number(logger, extract_box_reference(frame, REGISTERED_BOX));
    if (registered < 0){
        return std::nullopt;
    }
    return (size_t)registered;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
