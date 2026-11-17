/*  PokemonLZA AutoStory - Party slots detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The overworld party bar read by its empty slot frames, for a party too
 *  small to show the bar's D-pad arrows (OverworldPartySelectionDetector
 *  reads those, and they show only with 2+ Pokemon).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryPartySlotsDetector_H
#define PokemonAutomation_PokemonLZA_AutoStoryPartySlotsDetector_H

#include <array>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The party bar's empty slots at the bottom left: each a thin light frame,
//  66x66 on 1920x1080, over the scene darkened inside it. A party fills the
//  bar from the left, so the empty slots are its last ones; the bar shows
//  when its last slot (slot 6) is such a frame, for a party of 1 to 5.
//  Cutscenes and dialogs hide the bar.
class PartySlotsDetector : public StaticScreenDetector{
public:
    explicit PartySlotsDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    //  Each of the frame's four edges: the 2 px line, the strip inside it
    //  and the strip outside it.
    struct Edge{
        ImageFloatBox line;
        ImageFloatBox inside;
        ImageFloatBox outside;
    };

    //  Whether every edge of `frame` is lighter than the strip inside it
    //  and no darker than the strip outside it.
    static bool framed(const ImageViewRGB32& screen, const std::array<Edge, 4>& frame);

    Color m_color;
    //  The frame of the last slot (slot 6).
    ImageFloatBox m_last_slot;
    std::array<Edge, 4> m_last_frame;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
