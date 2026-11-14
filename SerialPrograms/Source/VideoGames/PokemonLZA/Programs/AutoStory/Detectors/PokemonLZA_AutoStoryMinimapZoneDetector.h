/*  PokemonLZA AutoStory - Minimap zone detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether the minimap shows the player inside a wild zone: its blue
 *  arrow on the zone's green.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMinimapZoneDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryMinimapZoneDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The middle of the minimap at the top left (a circle of radius 140
//  around (180, 180) on 1920x1080), where the player's blue arrow always
//  is: inside a wild zone the map there is the zone's translucent green,
//  in the city the map's beige and white. No text is read, so any language
//  matches. The minimap is not read when a scene or a menu hides it.
class MinimapZoneDetector : public StaticScreenDetector{
public:
    explicit MinimapZoneDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_middle;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
