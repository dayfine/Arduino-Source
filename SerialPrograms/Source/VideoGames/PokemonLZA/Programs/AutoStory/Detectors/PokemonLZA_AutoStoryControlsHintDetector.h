/*  PokemonLZA AutoStory - Controls hint detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The tutorial's controls hint under the player, a "?" card over
 *  "(L) Move" and "(R) Control Camera", which shows for ~10 s when the
 *  player gets control.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryControlsHintDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryControlsHintDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The controls hint the tutorial shows under the player when they get
//  control: its "?" card, sky blue with a dark "?", at x 947-971, y 872-900
//  on 1920x1080, centred, with no sky blue around it. Only the card is
//  read: the rows under it ("(L) Move", "(R) Control Camera") are centred
//  too, so their icons move with the text's length.
class ControlsHintDetector : public StaticScreenDetector{
public:
    explicit ControlsHintDetector(Color color = COLOR_BLUE);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_card;
    ImageFloatBox m_glyph;
    ImageFloatBox m_around[4];
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
