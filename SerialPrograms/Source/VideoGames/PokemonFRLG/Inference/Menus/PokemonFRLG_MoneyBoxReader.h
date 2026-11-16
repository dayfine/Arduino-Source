/*  Money Box Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Reads the money box a Mart's BUY menu shows on the top left: "MONEY",
 *  and below it the amount after the POKEDOLLAR sign, right-aligned
 *  (pokefirered money.c PrintMoneyAmountInMoneyBox).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_MoneyBoxReader_H
#define PokemonAutomation_PokemonFRLG_MoneyBoxReader_H

#include <cstddef>
#include <optional>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The money the box on `screen` (the whole frame) shows; nothing if it
//  can't be read.
std::optional<size_t> read_money_box(Logger& logger, const ImageViewRGB32& screen);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
