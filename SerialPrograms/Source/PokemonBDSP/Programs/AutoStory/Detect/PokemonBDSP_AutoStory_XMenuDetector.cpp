/*  BDSP AutoStory - X menu detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "PokemonBDSP/Inference/PokemonBDSP_MenuDetector.h"
#include "PokemonBDSP_AutoStory_XMenuDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


bool x_menu_bar(const ImageStats& bar){
    //  A flat sky blue: blue well over red, and nearly no texture.
    return bar.average.b > 200 && bar.average.r < 140 && bar.average.b - bar.average.r > 90 &&
        bar.stddev.sum() < 45;
}

XMenuDetector::XMenuDetector(Color color)
    : m_color(color)
{}
void XMenuDetector::make_overlays(VideoOverlaySet& items) const{
    items.add(m_color, X_MENU_BAR_BOX);
}
bool XMenuDetector::detect(const ImageViewRGB32& screen){
    if (x_menu_bar(image_stats(extract_box_reference(screen, X_MENU_BAR_BOX)))){
        return true;
    }
    return MenuDetector(m_color, true).detect(screen);
}

XMenuWatcher::XMenuWatcher(Color color)
    : XMenuDetector(color)
    , VisualInferenceCallback("XMenuWatcher")
{}
void XMenuWatcher::make_overlays(VideoOverlaySet& items) const{
    XMenuDetector::make_overlays(items);
}
bool XMenuWatcher::process_frame(const ImageViewRGB32& frame, WallClock){
    return detect(frame);
}


}
}
}
