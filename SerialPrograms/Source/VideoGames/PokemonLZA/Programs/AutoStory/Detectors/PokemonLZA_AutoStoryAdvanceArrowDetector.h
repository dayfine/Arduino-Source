/*  PokemonLZA AutoStory - Advance arrow detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The "press A to go on" chevron at the bottom right of a dialogue box or
 *  of the boxless subtitles of the ad early in the story.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryAdvanceArrowDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryAdvanceArrowDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The "press A to go on" chevron (a V about 32x19 on 1920x1080) at the
//  right end of a dialogue box and of boxless subtitles: dark on a white
//  box, white on a blue box and on the ad's subtitles. Found by shape in
//  x 1370-1480, y 930-1050: a blob of white (every channel 235+) or dark
//  (every channel 110 or under) pixels the size of the chevron, filling a
//  V, with no other pixel of its colour around it. Text is not read, so
//  any language matches.
class AdvanceArrowDetector : public StaticScreenDetector{
public:
    explicit AdvanceArrowDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_area;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
