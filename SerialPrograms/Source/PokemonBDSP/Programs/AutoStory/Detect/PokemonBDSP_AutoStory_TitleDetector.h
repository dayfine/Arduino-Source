/*  BDSP AutoStory - Title Detector
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  BDSP's title screen in any language: the logo over the legendary's
 *  silhouette, and under it the red line that asks for a press ("PUSH A
 *  BUTTON" in Japanese, "Appuyez sur le bouton A" in French). The logo
 *  is written in the game's language; the red line, its place and the
 *  dark bottom corners of the starfield are not.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TitleDetector_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TitleDetector_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Whether `screen` is the title with its red "press A" line: red text in
//  the band under the logo, and both bottom corners dark. False on an
//  empty image.
bool title_screen_shown(const ImageViewRGB32& screen);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
