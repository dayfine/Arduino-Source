/*  Battle HP Bar Reader
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The HP bars on a battle screen, the player's and the opponent's: their
 *  color and how much of them is filled.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_BattleHpBarReader_H
#define PokemonAutomation_PokemonFRLG_BattleHpBarReader_H

#include <optional>
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


enum class HpBarColor{
    GREEN,      //  over half
    YELLOW,     //  over a fifth
    RED,
};

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

//  The opponent's HP bar, in its box top left, the same way.
HpBarReading read_opponent_hp_bar(const ImageViewRGB32& game);

//  That bar's filled share on `screen` (the whole video, its Game Box
//  taken from the settings), none if no fill shows.
std::optional<double> read_opponent_hp_fill(const ImageViewRGB32& screen);



}
}
}
#endif
