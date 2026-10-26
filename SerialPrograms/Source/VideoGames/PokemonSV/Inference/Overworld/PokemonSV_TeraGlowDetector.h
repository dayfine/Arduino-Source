/*  Tera Glow Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_TeraGlowDetector_H
#define PokemonAutomation_PokemonSV_TeraGlowDetector_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  A static Tera Pokemon in the overworld: a purple orb (Rabsca's ball) inside a bright
//  halo. Positions are normalized to the frame.
struct TeraGlow{
    double x;
    double y;
    size_t orb_pixels;      //  Purple samples near the orb, on a 960x540 grid of 2.
    size_t halo_pixels;     //  Bright samples around it.
};

//  Looks in the middle band of the frame, left of the minimap. Calibrated on Savanna
//  frames (day and night, 2026-10-26): Rabsca gave 14-67 orb and 75-593 halo samples;
//  frames without it gave at most 1 and 6. The Savanna Plaza building has purple but
//  little halo (28, 17). A far Rabsca at night can be smaller, so the bar is 5 and 30.
std::optional<TeraGlow> find_tera_glow(const ImageViewRGB32& screen);


}
}
}
#endif
