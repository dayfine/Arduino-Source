/*  Name List Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The list of preset names shown for the rival: "I'll tell you...", then four names.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_NameListDetector_H
#define PokemonAutomation_PokemonBDSP_NameListDetector_H

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


class NameListDetector : public StaticScreenDetector{
public:
    NameListDetector(Color color = COLOR_MAGENTA);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

    //  The row the arrow is on: 0 is "I'll tell you...", 1 the first preset name.
    //  -1 if the list is not showing.
    int selected_row(const ImageViewRGB32& screen) const;

private:
    Color m_color;
    ImageFloatBox m_arrows;
    ImageFloatBox m_panel;
};


}
}
}
#endif
