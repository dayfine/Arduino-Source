/*  AutoStory Move Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story needs of moves, species and the type chart to choose an
 *  attack when the plan has none: each move's type, whether it does damage
 *  and its PP; each species' types (BattleData/moves.json, species.json,
 *  type_chart.json, from pret/pokefirered).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMoveData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMoveData_H

#include <cstddef>
#include <cstdint>
#include <string_view>
#include "Pokemon/Pokemon_Types.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{



//  The type enum, from the shared Pokemon code.
using Pokemon::PokemonType;

//  A move, as far as choosing an attack and tracking PP needs it.
struct MoveData{
    //  NONE for Curse's "???".
    PokemonType type = PokemonType::NONE;
    //  It does damage: any power, including moves whose damage doesn't
    //  come from their power (Flail, Dragon Rage).
    bool damaging = false;
    //  Full PP.
    uint8_t pp = 0;
};

//  A species' types; the same type twice for a species with one.
struct SpeciesData{
    PokemonType first = PokemonType::NONE;
    PokemonType second = PokemonType::NONE;
};

//  None for a move the data doesn't have.
const MoveData* move_data(std::string_view move);

//  None for a species the data doesn't have.
const SpeciesData* species_data(std::string_view species);

//  How well `attack` hits `defend`, in tenths: 20 is 2x, 5 is 0.5x.
size_t type_effectiveness(PokemonType attack, PokemonType defend);

//  The type `species` may be immune to through its ability (Levitate:
//  Ground; Flash Fire: Fire; Volt Absorb: Electric; Water Absorb: Water),
//  for the species that can have one in Gen 3, as its only ability or
//  either of two; NONE for any other species.
PokemonType ability_immunity(std::string_view species);

//  `move` takes a turn to charge or to dig or fly up (Dig, Fly, Solar Beam,
//  Skull Bash, Razor Wind, Sky Attack), so it hits half as often as another
//  move of its strength.
bool takes_two_turns(std::string_view move);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
