/*  RNG Method 1
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The Gen 3 LCRNG and the Method 1 Pokemon it generates: PID, IVs, shininess
 *  and Hidden Power.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngMethod1_H
#define PokemonAutomation_PokemonFRLG_RngMethod1_H

#include <cstdint>
#include <string>
#include "Pokemon/Pokemon_AdvRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  The Gen 3 LCRNG: state * 0x41C64E6D + 0x6073.
uint32_t lcrng_next(uint32_t state);
//  Steps the Gen 3 LCRNG backward by one advance.
uint32_t lcrng_previous(uint32_t state);
//  How many lcrng_next() calls take "from" to "to". The LCRNG has a single cycle of
//  2^32 states, so every pair has exactly one distance.
uint32_t lcrng_distance(uint32_t from, uint32_t to);

//  The static, gift or starter Pokemon (Method 1) generated from "state", the RNG state
//  just before the PID is rolled: the next two calls give the low and high PID halves,
//  the two after that the IVs (HP/Atk/Def, then Spe/SpA/SpD).
Pokemon::AdvPokemonResult method1_pokemon(uint32_t state);

//  Determines shiny type (none, star, square) from PID and trainer IDs.
Pokemon::AdvShinyType shiny_type(uint32_t pid, uint16_t tid, uint16_t sid);

//  Calculated Hidden Power typing and base power.
struct HiddenPower{
    std::string type;
    uint8_t power;
};
//  Calculates Hidden Power attributes from individual values.
HiddenPower hidden_power(const Pokemon::AdvIVs& ivs);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
