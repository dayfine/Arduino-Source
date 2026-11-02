/*  BDSP AutoStory - Indoors
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Whether the screen shows a room: the walker leaves a building by it.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_Indoors_H
#define PokemonAutomation_PokemonBDSP_AutoStory_Indoors_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Whether the screen shows a building's inside: its warm walls and floor
//  fill much of the view (Pokemon Centers, the Trainers' School: 42..65% of
//  the screen), and the room floats in black, which shows along at least one
//  edge (15% or more of a strip 12% deep). Warmth alone is not enough: the
//  evening tint makes outside 50..72% warm (r256, r257, Sandgem), and
//  Oreburgh's ground 58..69% by day. Of 1412 lab screenshots, the rule calls
//  every outside one outside, at all hours, and the rooms rooms. A fade is
//  dark everywhere but not warm, so it is not inside.
bool indoors(const ImageViewRGB32& screen);


}
}
}
#endif
