/*  PokemonLZA AutoStory - Caught mark reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether the wild Pokemon locked on to is of a species already caught:
 *  its name plate then shows a Poke Ball after the level and gender.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryCaughtMarkReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryCaughtMarkReader_H

#include <optional>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonTools/VisualDetector.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  While ZL locks on to a wild Pokemon: true when its name plate shows the
//  caught mark, false when the plate shows none, nothing when no plate was
//  found. The plate is found by its HP bar, a green bar (about 45, 198, 115)
//  at least 80 px wide on 1920x1080, in the upper two thirds of the
//  screen; the mark, pinkish red over white, sits above the bar's right end
//  (0.6-1.5 bar widths from its left, 0.45-0.85 above it), right of the
//  orange gender mark. Tuned on Mareep, caught and not, and
//  Bunnelby and Scatterbug, on the console. The plate shows
//  whether or not the partner is out; the move symbols only when it is.
std::optional<bool> lock_target_caught(const ImageViewRGB32& screen);

//  While ZL locks on to a wild Pokemon: its name plate's HP bar (as
//  above) as shares of the screen, nothing when no plate was found. The
//  plate stands above the target, so a lower bar is a nearer target.
std::optional<ImageFloatBox> lock_target_plate_bar(const ImageViewRGB32& screen);

//  Whether ZL holds a lock on a wild Pokemon: its name plate shows
//  (lock_target_plate_bar()). Only the bar's shape and color are read, so
//  any language matches.
class LockTargetPlateDetector : public StaticScreenDetector{
public:
    explicit LockTargetPlateDetector(Color color = COLOR_GREEN);

    virtual void make_overlays(VideoOverlaySet& items) const override;
    virtual bool detect(const ImageViewRGB32& screen) override;

private:
    Color m_color;
};



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
