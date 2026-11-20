/*  PokemonLZA AutoStory - Dex registered detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Detector for the Pokedex "Registered!" banner after catching a new species.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryDexRegisteredDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryDexRegisteredDetector_H

#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  Detects the new-species Pokedex registration banner.
class DexRegisteredDetector : public StaticScreenDetector{
public:
    explicit DexRegisteredDetector(Color color = COLOR_CYAN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
    ImageFloatBox m_left_arc;
    ImageFloatBox m_right_arc;
    ImageFloatBox m_tag;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
