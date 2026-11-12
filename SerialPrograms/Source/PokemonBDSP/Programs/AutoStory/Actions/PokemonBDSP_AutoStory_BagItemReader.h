/*  BDSP AutoStory - Bag item reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads an item name in the Bag's list against the items the story uses,
 *  in every game language. Returns the item's slug ("potion"), or "".
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_BagItemReader_H
#define PokemonAutomation_PokemonBDSP_AutoStory_BagItemReader_H

#include <string>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The slug of the story item named in `image` (one Bag row's name box), or
//  "" when it is none of them.
std::string read_bag_item_slug(Logger& logger, Language language, const ImageViewRGB32& image);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
