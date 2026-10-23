/*  Map Fly Point Label Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Read the name label the map shows above the cursor when the cursor is on
 *  a fly point.
 *
 */

#ifndef PokemonAutomation_PokemonSV_MapFlyPointLabelReader_H
#define PokemonAutomation_PokemonSV_MapFlyPointLabelReader_H

#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/Language.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


class MapFlyPointLabelReader{
public:
    MapFlyPointLabelReader();

    void make_overlays(VideoOverlaySet& items) const;

    //  OCR the label's dark text. Returns the raw OCR text, which is empty
    //  (or noise) when there is no label.
    std::string read(Logger& logger, Language language, const ImageViewRGB32& screen) const;

private:
    ImageFloatBox m_box;
};


}
}
}
#endif
