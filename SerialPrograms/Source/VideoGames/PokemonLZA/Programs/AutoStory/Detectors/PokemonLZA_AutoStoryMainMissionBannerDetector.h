/*  PokemonLZA AutoStory - Main mission banner detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The "MAIN MISSION 01" banner across the middle when a main mission
 *  starts.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMainMissionBannerDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryMainMissionBannerDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The banner a main mission starts with: a see-through yellow bar at
//  x 345-1590, y 410-600 on 1920x1080 with the mission's name in the
//  middle, and above it, left of "MAIN MISSION 01", a white hexagon with
//  an orange "!" at about x 700-738, y 327-370. The bar is read left and
//  right of the name, with no yellow just above or below it, and the icon
//  is looked for along the header (the header's text is centred). Text is
//  not read, so any language matches.
class MainMissionBannerDetector : public StaticScreenDetector{
public:
    explicit MainMissionBannerDetector(Color color = COLOR_ORANGE);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_bar_left;
    ImageFloatBox m_bar_right;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
    ImageFloatBox m_icon_left;
    ImageFloatBox m_icon_right;
    ImageFloatBox m_icon_stroke;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
