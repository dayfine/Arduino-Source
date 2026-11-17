/*  PokemonLZA AutoStory - Battle markers detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The row of five markers right of the lead's panel's gender icon in a
 *  battle: one blue triangle and white dots. The panel itself
 *  (BattlePanelDetector) shows all through a wild zone, in battle or not
 *  (console B, 2026-11-16); the markers show only in a battle.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBattleMarkersDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryBattleMarkersDetector_H

#include <vector>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The markers' five slots, 16 x 12 on 1920x1080, centred at x 405, 432,
//  459, 486 and 513, y 806. A battle: one slot the triangle's blue (about
//  15, 180, 255) over 20% of its pixels, and 3 or more others a dot's
//  white over 6% and under 50% (a white scene fills a slot) with no blue.
//  Measured on Taunie's battle (03.02b) and a wild Mareep's (03.08b),
//  neither locked on: triangle 0.39 and 0.44, dots 0.18 to 0.57; no
//  other of 1403 PLZA frames (CommandLineTests and lab-B's) has them.
class BattleMarkersDetector : public StaticScreenDetector{
public:
    explicit BattleMarkersDetector(Color color = COLOR_BLUE);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    std::vector<ImageFloatBox> m_slots;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
