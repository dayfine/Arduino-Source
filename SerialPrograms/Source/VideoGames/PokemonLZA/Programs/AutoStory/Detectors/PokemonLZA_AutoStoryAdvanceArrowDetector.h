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


//  The chevron (a V about 32x19 on 1080p) at the right end of a dialogue box or
//  subtitles, dark on white or white on blue. Found by shape in x 1370-1480,
//  y 930-1050: a white or dark blob filling a V with nothing of its colour around it.
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
