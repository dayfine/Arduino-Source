/*  PokemonLZA AutoStory - Skip prompt detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The opening cutscene's "(+) Hold button to skip" badge.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStorySkipPromptDetector_H
#define PokemonAutomation_PokemonLZA_AutoStorySkipPromptDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"
#include "VideoGames/PokemonLZA/Inference/PokemonLZA_ButtonDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The cutscene's "(+) Hold button to skip" badge at the top right: the
//  (+) icon (ButtonDetector's ButtonType::ButtonPlus), white text on the
//  badge, and the badge's dark, even strip under the text.
class SkipPromptDetector : public StaticScreenDetector{
public:
    explicit SkipPromptDetector(Color color = COLOR_MAGENTA);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ButtonDetector m_icon;
    ImageFloatBox m_text;
    ImageFloatBox m_strip;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
