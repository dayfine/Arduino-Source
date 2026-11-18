/*  Overworld Warp Battle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_OverworldWarpBattle_H
#define PokemonAutomation_PokemonFRLG_OverworldWarpBattle_H

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  What a wild battle after a warp's fade says about the warp.
enum class WarpBattle{
    NONE,       //  no battle: the warp was taken
    INSTEAD,    //  the fade was the battle's wipe: no warp
    //  The new map's overworld showed before the battle: the warp was taken,
    //  and the battle started there, a step or more on. Victory Road 3F's
    //  ladder, then a wild battle on 3F (console B, 2026-11-16 02:18, 19.04c).
    AFTER,
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
