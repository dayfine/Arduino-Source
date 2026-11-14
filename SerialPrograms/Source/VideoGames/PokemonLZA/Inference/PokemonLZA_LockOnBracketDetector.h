/*  Lock-On Bracket Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  In the overworld, the game marks the wild Pokemon that ZL would lock on
 *  to with two thin white arcs, "( )", one each side of it, before
 *  anything is pressed. One Pokemon at a time has them; they move to
 *  another as the Pokemon walk about. The nearer the Pokemon, the taller
 *  the arcs (console B, 2026-11-13).
 *
 */

#ifndef PokemonAutomation_PokemonLZA_LockOnBracketDetector_H
#define PokemonAutomation_PokemonLZA_LockOnBracketDetector_H

#include <optional>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/VideoPipeline/VideoOverlayScopes.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The pair of arcs around one wild Pokemon. All values are shares of the
//  screen (0 to 1).
struct LockOnBrackets{
    //  The middle between the arcs.
    double x = 0;
    double y = 0;
    //  The taller arc's height, a share of the screen's height: the nearer
    //  the Pokemon, the larger.
    double height = 0;
    //  From the left arc's left edge to the right arc's right edge.
    ImageFloatBox box;
};

//  The brackets on `screen`, the tallest pair when there are more; nothing
//  when there are none.
std::optional<LockOnBrackets> find_lock_on_brackets(const ImageViewRGB32& screen);


class LockOnBracketDetector : public StaticScreenDetector{
public:
    LockOnBracketDetector(Color color, VideoOverlay* overlay);

    virtual void make_overlays(VideoOverlaySet& items) const override;

    //  Whether brackets show; the pair found is kept for last_detected().
    virtual bool detect(const ImageViewRGB32& screen) override;

    const std::optional<LockOnBrackets>& last_detected() const{
        return m_last_detected;
    }

private:
    const Color m_color;
    VideoOverlay* m_overlay;
    std::optional<LockOnBrackets> m_last_detected;
    std::optional<OverlayBoxScope> m_last_detected_box;
};


void add_tests_LockOnBracketDetector(UnitTestDatabase& database);


}
}
}
#endif
