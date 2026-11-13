/*  BDSP AutoStory - Trainer Card Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonBDSP_AutoStory_TrainerCardReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


Pokemon::BadgeReading read_trainer_card_badge(const ImageViewRGB32&, size_t){
    //  See the header: the badge boxes wait for a fixture.
    return Pokemon::BadgeReading::UNREADABLE;
}



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
