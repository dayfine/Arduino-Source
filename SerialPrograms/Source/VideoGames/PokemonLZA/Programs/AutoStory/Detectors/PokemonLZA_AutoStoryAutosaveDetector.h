/*  PokemonLZA AutoStory - Autosave detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The autosave badge, a pencil and "Saving...", which shows for 1-3 s
 *  after the game autosaves.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryAutosaveDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryAutosaveDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The autosave badge at the top right: a white pencil with a cyan glow at
//  x 1603-1630, y 73-104 on 1920x1080, and nothing white or cyan just left
//  of it, above it or below it. Only the icon is read, so any language
//  matches.
class AutosaveDetector : public StaticScreenDetector{
public:
    explicit AutosaveDetector(Color color = COLOR_YELLOW);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_icon;
    ImageFloatBox m_left;
    ImageFloatBox m_above;
    ImageFloatBox m_below;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
