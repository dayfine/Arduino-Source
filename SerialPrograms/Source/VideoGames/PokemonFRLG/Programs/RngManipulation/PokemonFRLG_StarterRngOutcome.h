/*  Starter RNG Outcome
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one Starter RNG attempt ended with, and how a shiny is identified
 *  from the seed and advances it hit.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_StarterRngOutcome_H
#define PokemonAutomation_PokemonFRLG_StarterRngOutcome_H

#include <cstdint>
#include <vector>
#include "Pokemon/Pokemon_AdvRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Result outcome of an individual starter attempt.
enum class StarterRngOutcome{
    shiny,          //  The starter was shiny after a reset; not known if on the target.
    shiny_on_target,    //  Shiny, and the target is among the states its stats match.
    shiny_off_target,   //  Shiny, and the states its stats match aren't the target.
    target_hit,     //  The calibration shows the target was hit; not shiny.
    wild_shiny,     //  A wild shiny on Route 1, with ignore_wild_shinies off.
    out_of_resets,  //  More resets than max_resets.
    no_matches,     //  5 searches in a row matched no state near the target.
};

//  A shiny's outcome from the states its stats match near the target
//  ("hits"): on the target if it is among them, off it if not, and just
//  shiny if none match.
StarterRngOutcome identify_shiny(uint16_t seed, uint64_t advances, const std::vector<AdvRngState>& hits);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
