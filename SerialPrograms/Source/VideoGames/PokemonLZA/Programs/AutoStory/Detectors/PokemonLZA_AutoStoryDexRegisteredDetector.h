/*  PokemonLZA AutoStory - Dex registered detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The banner after catching a new species: a cyan ring around the Pokemon,
 *  "Registered!" over it and a "No.###" tag under it.
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


//  The ring's cyan arcs (x 852-872 and 1100-1120, y 140-220 on 1080p) and the slate
//  "No.###" tag (x 845-945, y 268-298) with white text. Matched Mareep's 2 frames and
//  none of 553 others.
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
