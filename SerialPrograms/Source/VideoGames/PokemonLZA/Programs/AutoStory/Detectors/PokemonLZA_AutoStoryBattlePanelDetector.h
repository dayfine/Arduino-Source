/*  PokemonLZA AutoStory - Battle panel detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The player's Pokemon's panel at the bottom left in a battle: its level
 *  badge ("L. 7") and HP bar. It shows the whole battle, locked on or not;
 *  the move list and its hints show only while the player is locked on.
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


//  The panel's level badge, sky blue (about 140, 210, 235) at x 240-320,
//  y 799-818 on 1920x1080, over half its pixels; and its HP bar at x
//  236-440, y 882-902 (left of the "4/24"), over 80% its HP color or its
//  grey trough (about 95, 95, 95). Taunie's battle not locked on (03.02b,
//  console B 2026-11-16): badge 0.78, bar 0.95; overworld frames: badge
//  0.18 at most, bar 0.20 at most.
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
