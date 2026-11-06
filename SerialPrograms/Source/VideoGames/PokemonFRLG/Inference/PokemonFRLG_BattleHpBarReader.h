/*  Battle HP Bar Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The HP bar in the player's box on a battle screen: its color and how
 *  much of it is filled.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BattleHpBarReader_H
#define PokemonAutomation_PokemonFRLG_BattleHpBarReader_H

#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Color classification of an HP bar.
enum class HpBarColor{
    GREEN,      //  over half
    YELLOW,     //  over a fifth
    RED,
};

//  Parsed health bar reading indicating fraction and health state color.
struct HpBarReading{
    //  Some of the bar is filled.
    bool seen = false;
    HpBarColor color = HpBarColor::GREEN;
    //  The filled share of the bar's width, 0 to 1: the HP to within a
    //  pixel of the 48-pixel bar.
    double fill = 0;
};

//  `game`: the game's screen, the Game Box of the video.
HpBarReading read_player_hp_bar(const ImageViewRGB32& game);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
