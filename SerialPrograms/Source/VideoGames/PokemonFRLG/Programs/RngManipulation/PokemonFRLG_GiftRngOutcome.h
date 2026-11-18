/*  Gift RNG Outcome
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What one Gift RNG attempt ended with, and how a shiny is identified from
 *  the seed and advances it hit.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_GiftRngOutcome_H
#define PokemonAutomation_PokemonFRLG_GiftRngOutcome_H

#include <cstdint>
#include <vector>
#include "Pokemon/Pokemon_AdvRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The outcome of an individual Gift RNG reset attempt.
enum class GiftRngOutcome{
    shiny,              //  The gift was shiny after a reset; not known if on the target.
    shiny_on_target,    //  Shiny, and the target is among the states its stats match.
    shiny_off_target,   //  Shiny, and the states its stats match aren't the target.
    target_hit,         //  The calibration shows the target was hit; not shiny.
    out_of_resets,      //  More resets than max_resets.
    no_matches,         //  5 searches in a row matched no state near the target.
};

//  A shiny gift's outcome from the states its stats match near the target
//  ("hits"): on the target if it is among them, off it if not, and just
//  shiny if none match.
GiftRngOutcome gift_shiny_outcome(uint16_t seed, uint64_t advances, const std::vector<AdvRngState>& hits);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
