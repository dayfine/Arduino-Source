/*  AutoStory Stats
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A Pokemon's stats as POKEMON SKILLS shows them.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryStats_H
#define PokemonAutomation_PokemonFRLG_AutoStoryStats_H

#include <cstdint>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A Pokemon's stats, in the game's order.
struct Stats{
    uint16_t hp = 0;
    uint16_t attack = 0;
    uint16_t defense = 0;
    uint16_t sp_attack = 0;
    uint16_t sp_defense = 0;
    uint16_t speed = 0;
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
