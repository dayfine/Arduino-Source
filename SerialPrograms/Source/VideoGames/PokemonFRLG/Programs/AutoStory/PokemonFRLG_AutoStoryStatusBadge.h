/*  AutoStory Status Badge
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The status badge left of the player's HP numbers in a battle, read by
 *  its mean color.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStatusBadge_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStatusBadge_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "PokemonFRLG_AutoStorySupplies.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The status badge left of the player's HP numbers, by its mean color.
StatusCondition read_status_badge(const ImageViewRGB32& badge);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
