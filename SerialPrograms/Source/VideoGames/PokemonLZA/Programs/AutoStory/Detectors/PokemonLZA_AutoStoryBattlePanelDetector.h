/*  PokemonLZA AutoStory - Battle panel detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The player's Pokemon's panel at the bottom left in a battle, shown the whole
 *  battle, locked on or not.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBattlePanelDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryBattlePanelDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The level badge's blue (dim_cyan_share()) at x 240-320, y 799-818 on 1080p over half
//  its pixels, and the HP bar at x 236-440, y 882-902 over 80% fill or grey trough. The
//  badge is translucent: over grass and paving stone it is as dark as (80, 170, 190), with
//  wider white "Nv." text in Spanish. In battle: badge 0.78-0.88, bar 0.88-0.95. The panel
//  also shows in a wild zone, and 68 of 1203 PLZA test frames read as it: pair it with
//  BattleMarkersDetector.
class BattlePanelDetector : public StaticScreenDetector{
public:
    explicit BattlePanelDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_badge;
    ImageFloatBox m_bar;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
