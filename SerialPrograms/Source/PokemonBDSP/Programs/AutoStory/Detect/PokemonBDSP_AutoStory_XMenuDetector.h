/*  BDSP AutoStory - X menu detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The X menu at every stage of the game. MenuDetector wants the full menu's
 *  white columns between the icons; a new game's menu has two icons (BAG,
 *  and OPTIONS with a "NEW" tag) and was missed (r5, 2026-11-01). The blue
 *  bar along the bottom ("A Confirm  R Save  B Close") is there from the
 *  first menu on.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_XMenuDetector_H
#define PokemonAutomation_PokemonBDSP_AutoStory_XMenuDetector_H

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTools/ImageStats.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The bottom bar's left end, left of its button prompts.
inline const ImageFloatBox X_MENU_BAR_BOX(0.02, 0.965, 0.18, 0.025);
//  Whether `bar` (the stats of X_MENU_BAR_BOX) is the menu's flat blue bar.
bool x_menu_bar(const ImageStats& bar);

class XMenuDetector : public StaticScreenDetector{
public:
    XMenuDetector(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    //  The bottom bar, or the full menu as MenuDetector (with the NEW tag
    //  skipped) sees it.
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
};

class XMenuWatcher : public XMenuDetector, public VisualInferenceCallback{
public:
    XMenuWatcher(Color color = COLOR_RED);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;
};


}
}
}
#endif
