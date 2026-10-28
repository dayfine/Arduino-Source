/*  Selection Arrow
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_SelectionArrow_H
#define PokemonAutomation_PokemonBDSP_SelectionArrow_H

#include <deque>
#include <vector>
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/InferenceCallbacks/VisualInferenceCallback.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The menu selection arrows in `image`, in its pixel coordinates.
std::vector<ImagePixelBox> find_selection_arrows(const ImageViewRGB32& image);

class SelectionArrowFinder : public VisualInferenceCallback{
public:
    SelectionArrowFinder(
        VideoOverlay& overlay,
        const ImageFloatBox& box,
        Color color
    );

    bool detect(const ImageViewRGB32& screen);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool process_frame(const ImageViewRGB32& frame, WallClock timestamp) override;

protected:
    VideoOverlay& m_overlay;
    Color m_color;
    ImageFloatBox m_box;
    std::deque<OverlayBoxScope> m_arrow_boxes;

private:
    size_t m_trigger_count = 0;
};



}
}
}
#endif
