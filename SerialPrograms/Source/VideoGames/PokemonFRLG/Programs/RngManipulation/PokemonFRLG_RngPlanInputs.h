/*  RNG Plan Inputs
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The TID, IVs and natures an RNG plan is typed in with, as text.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_RngPlanInputs_H
#define PokemonAutomation_PokemonFRLG_RngPlanInputs_H

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "Pokemon/Pokemon_AdvRng.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A TID or SID typed as a number from 0 to 65535. Nothing for any other text.
std::optional<uint16_t> parse_trainer_id(const std::string& text);

//  "HP/Atk/Def/SpA/SpD/Spe", as Ten Lines and the RNG programs list IVs.
//  Nothing for text that isn't six numbers from 0 to 31.
std::optional<Pokemon::AdvIVs> parse_ivs(const std::string& text);
//  Formats IV values as a comma-separated string.
std::string ivs_to_string(const Pokemon::AdvIVs& ivs);

//  Natures separated by commas ("Modest, Timid"). Empty text is any nature
//  (no entries). Nothing for a name that isn't a nature.
std::optional<std::vector<Pokemon::AdvNature>> parse_natures(const std::string& text);


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
