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


//  The card a main mission ends with, which waits for A: a big green
//  "COMPLETE" at x 615-1300, y 271-349 on 1920x1080 over the mission's
//  yellow bar (x 345-1590, y 390-585, "MAIN MISSION 01" and the mission's
//  name), in a yellow hexagon with the mission's item. The word is read
//  with nothing green just above or below it, and the bar left and right
//  of the name. The word is the same in every language seen (the header
//  stays English in Chinese), so its text is not read.
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
