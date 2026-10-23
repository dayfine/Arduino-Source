/*  Map Fly Point Label Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTypes/ImageRGB32.h"
#include "CommonTools/Images/ImageFilter.h"
#include "CommonTools/OCR/OCR_Routines.h"
#include "PokemonSV_MapFlyPointLabelReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  The label is a white box centered above the cursor, which sits at the
//  middle of the screen. On a lab screenshot (2026-10-22, the zoom level the
//  map opens with) the box for 中心廣場 spans x 0.426-0.574, y 0.341-0.408.
//  The read area keeps inside it vertically, and is wider so that longer
//  names fit.
MapFlyPointLabelReader::MapFlyPointLabelReader()
    : m_box(0.250, 0.348, 0.500, 0.054)
{}

void MapFlyPointLabelReader::make_overlays(VideoOverlaySet& items) const{
    items.add(COLOR_BLUE, m_box);
}

std::string MapFlyPointLabelReader::read(Logger& logger, Language language, const ImageViewRGB32& screen) const{
    //  Keep the label's dark text and blank everything else, including most
    //  of the map around the label.
    ImageRGB32 filtered = to_blackwhite_rgb32_range(
        extract_box_reference(screen, m_box),
        true,
        0xff000000, 0xff707070
    );

    std::string ret;
    for (char ch : OCR::ocr_read(language, filtered)){
        if ((unsigned char)ch < 32){
            continue;
        }
        ret += ch;
    }
    logger.log("Map fly point label: \"" + ret + "\"");
    return ret;
}


}
}
}
