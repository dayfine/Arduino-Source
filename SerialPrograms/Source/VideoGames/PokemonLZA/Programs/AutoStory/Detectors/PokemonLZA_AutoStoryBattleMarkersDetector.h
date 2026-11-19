/*  PokemonLZA AutoStory - Battle markers detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The row of five markers right of the lead's panel's gender icon, shown only in a
 *  battle (the panel itself shows all through a wild zone).
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


//  Five 16 x 12 slots at x 405-513 (27 apart), y 806 on 1080p. A battle: one slot over
//  20% triangle blue, and 3+ others 6-50% white with no blue. Measured: triangle
//  0.39-0.44, dots 0.18-0.57; no other of 1403 PLZA frames has them.
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
