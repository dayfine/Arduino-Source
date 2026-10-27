/*  Pride Rock Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonSV_PrideRockDetector_H
#define PokemonAutomation_PokemonSV_PrideRockDetector_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonSV{


//  The red of Pyroar's mane: its x position across the frame (0 to 1), or none.
//  Leaves out the player's red backpack and the minimap (overworld), or photo
//  mode's button guide and toolbar.
std::optional<double> find_red_mane_x(const ImageViewRGB32& image, bool photo_mode);

//  Whether the player missed Pride Rock: grass under the player's feet, or some
//  grass with no Pyroar in view. Calibrated on console frames from 2026-10-26
//  (15 on the rock, 7 off it, day, night and rain).
bool off_pride_rock(const ImageViewRGB32& image);


}
}
}
#endif
