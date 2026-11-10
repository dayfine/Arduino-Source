/*  PokemonLZA AutoStory - Badge detectors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Badges at the top right of the screen:
 *  - the opening cutscene's "(+) Hold button to skip" (SkipPromptDetector);
 *  - the autosave badge, a pencil and "Saving..." (AutosaveDetector), which
 *    shows for 1-3 s after the game autosaves.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryBadgeDetectors_H
#define PokemonAutomation_PokemonLZA_AutoStoryBadgeDetectors_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The cutscene's "(+) Hold button to skip" badge at the top right: the
//  white (+) icon, white text on the badge, and the badge's dark, even
//  strip under the text.
class SkipPromptDetector : public StaticScreenDetector{
public:
    explicit SkipPromptDetector(Color color = COLOR_MAGENTA);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_icon;
    ImageFloatBox m_text;
    ImageFloatBox m_strip;
};


//  The autosave badge at the top right: a white pencil with a cyan glow at
//  x 1603-1630, y 73-104 on 1920x1080, and nothing white or cyan just left
//  of it. Only the icon is read, so any language matches.
class AutosaveDetector : public StaticScreenDetector{
public:
    explicit AutosaveDetector(Color color = COLOR_YELLOW);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_icon;
    ImageFloatBox m_left;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
