/*  Place Name Banner Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonSV_PlaceNameBannerReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  The banner is centered horizontally. On a lab screenshot (2026-10-22,
//  Chinese (Traditional), landing at Central Plaza) the name 中心廣場 spans
//  x 0.438-0.562, y 0.167-0.222, with the blue line under it at y 0.233 and
//  the smaller "Terarium" subtitle below that. The read area stays above the
//  line and is wider than the name, so that longer names fit.
PlaceNameBannerReader::PlaceNameBannerReader()
    : m_box(0.300, 0.155, 0.400, 0.070)
{}

void PlaceNameBannerReader::make_overlays(VideoOverlaySet& items) const{
    items.add(COLOR_BLUE, m_box);
}

std::vector<std::string> PlaceNameBannerReader::read(Logger& logger, Language language, const ImageViewRGB32& screen) const{
    //  The name is white, drawn straight over the scene. Keep only the
    //  brightest pixels; the scene behind it changes with the time of day,
    //  so try a few thresholds.
    static const uint32_t MINS[] = {0xffe0e0e0, 0xffc0c0c0};

    const ImageViewRGB32 cropped = extract_box_reference(screen, m_box);
    std::vector<std::string> ret;
    for (uint32_t mins : MINS){
        ImageRGB32 filtered = to_blackwhite_rgb32_range(cropped, true, mins, 0xffffffff);
        std::string text;
        for (char ch : OCR::ocr_read(language, filtered)){
            if ((unsigned char)ch < 32){
                continue;
            }
            text += ch;
        }
        logger.log("Place name banner: \"" + text + "\"");
        ret.emplace_back(std::move(text));
    }
    return ret;
}


}
}
}
