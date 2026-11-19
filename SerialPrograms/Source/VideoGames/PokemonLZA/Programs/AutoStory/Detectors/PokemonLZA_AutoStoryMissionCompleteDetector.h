/*  PokemonLZA AutoStory - Mission complete detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The "COMPLETE" card when a main mission ends, which waits for A.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMissionCompleteDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryMissionCompleteDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  A big green "COMPLETE" (x 615-1300, y 271-349 on 1080p) over the mission's yellow
//  bar, with nothing green just above or below. The word is the same in every language
//  seen, so text is not read.
class MissionCompleteDetector : public StaticScreenDetector{
public:
    explicit MissionCompleteDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_word;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
    ImageFloatBox m_bar_left;
    ImageFloatBox m_bar_right;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
