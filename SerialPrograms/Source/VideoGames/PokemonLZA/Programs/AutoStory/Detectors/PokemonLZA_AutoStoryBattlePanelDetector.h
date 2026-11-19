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


//  The level badge sky blue (about 140, 210, 235) at x 240-320, y 799-818 on 1080p over
//  half its pixels, and the HP bar at x 236-440, y 882-902 over 80% fill or grey trough.
//  In battle: badge 0.78, bar 0.95; overworld: at most 0.18 and 0.20.
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
