/*  PokemonLZA AutoStory - Dex registered detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The banner after catching a species the Pokedex did not have: a cyan
 *  ring at the top middle around the Pokemon, "Registered!" over it and
 *  its "No.###" tag under it. A repeat catch shows none.
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


//  The ring's left and right arcs cyan (x 852-872 and 1100-1120, y 140-220
//  on 1920x1080) and the slate "No.###" tag under it (x 845-945, y
//  268-298) with its white text. Only shapes and colors are read, so any
//  language matches. Tuned on Mareep's banner (console B, 2026-11-10): the
//  rule matched its 2 frames and none of 553 other lab frames.
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
