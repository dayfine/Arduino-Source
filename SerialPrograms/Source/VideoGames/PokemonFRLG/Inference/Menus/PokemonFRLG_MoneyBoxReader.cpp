/*  Money Box Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/ImageTools/ImageBoxes.h"
#include "CommonTools/OCR/OCR_NumberReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Settings.h"
#include "PokemonFRLG_MoneyBoxReader.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The amount's digits in the game's 240 x 160 screen, from right of the
//  POKEDOLLAR sign to the box's right edge: x 44.5 to 75, y 22 to 33
//  (measured on a Fuchsia Mart BUY list capture, "MONEY / P 10586":
//  digits at x 47..71, y 24..31).
const ImageFloatBox MONEY_DIGITS(44.5 / 240, 22.0 / 160, 30.5 / 240, 11.0 / 160);
//  The most the game holds (money.c MAX_MONEY).
constexpr int MAX_MONEY = 999999;

} // namespace


std::optional<size_t> read_money_box(Logger& logger, const ImageViewRGB32& screen){
    if (screen.width() == 0 || screen.height() == 0){
        return std::nullopt;
    }
    const ImageViewRGB32 game = extract_box_reference(screen, GameSettings::instance().GAME_BOX);
    const int money = OCR::read_number(logger, extract_box_reference(game, MONEY_DIGITS));
    if (money < 0 || money > MAX_MONEY){
        return std::nullopt;
    }
    return (size_t)money;
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
