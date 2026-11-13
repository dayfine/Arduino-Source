/*  BDSP AutoStory - Trainer Card Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the badges on the Trainer Card (menu icon CARD).
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TrainerCardReader_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TrainerCardReader_H

#include <cstddef>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Pokemon/AutoStory/Types/Pokemon_BadgeCheck.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Whether badge `index` (0 = Coal Badge, in the order the gyms are beaten)
//  is on the card shown on `screen`.
//
//  TODO: no Trainer Card screenshot exists yet (CommandLineTests has none and
//  the lab has only save-prompt shots), so the badge boxes are not known and
//  this reads UNREADABLE, which fails the thorough check (nothing is passed
//  unseen): until it is built 01.04 cannot pass at those levels. verify_badge() saves the card it opened to
//  Screenshots/AutoStory/cards/; a card with 0 badges and one with 1 badge
//  from console B are the fixtures to draw the boxes and thresholds from.
Pokemon::BadgeReading read_trainer_card_badge(const ImageViewRGB32& screen, size_t index);



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
