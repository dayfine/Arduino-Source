/*  PokemonLZA AutoStory - Start screen reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryStartScreenReader_H
#define PokemonAutomation_PokemonLZA_AutoStoryStartScreenReader_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryStarts.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  The overworld signature fields on `screen`: talk_prompt, party (bar shows) and
//  minimap (matching_minimap()); a fading screen reads nothing. wild_zone needs the
//  full map: wild_zone_from_map().
StartSignature read_start_screen(const ImageViewRGB32& screen);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
