/*  Place Name Banner Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Read the place name the game shows near the top of the screen after
 *  landing from a fly, e.g. "Central Plaza" over a blue line.
 *
 */

#ifndef PokemonAutomation_PokemonSV_PlaceNameBannerReader_H
#define PokemonAutomation_PokemonSV_PlaceNameBannerReader_H

#include <string>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


class PlaceNameBannerReader{
public:
    PlaceNameBannerReader();

    void make_overlays(VideoOverlaySet& items) const;

    //  OCR the banner's white text, once per brightness threshold. Returns
    //  the raw OCR text of each read, which is empty (or noise) when there
    //  is no banner.
    std::vector<std::string> read(Logger& logger, Language language, const ImageViewRGB32& screen) const;

private:
    ImageFloatBox m_box;
};


}
}
}
#endif
