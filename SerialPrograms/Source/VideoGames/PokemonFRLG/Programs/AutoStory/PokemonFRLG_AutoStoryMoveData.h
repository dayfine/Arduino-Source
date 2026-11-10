/*  AutoStory Move Data
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Moves, species and the type chart, as pret/pokefirered has them
 *  (BattleData/moves.json, species.json, type_chart.json).
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryMoveData_H
#define PokemonAutomation_PokemonFRLG_AutoStoryMoveData_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Logging/AbstractLogger.h"
#include "Common/Cpp/TestRunners/UnitTestDatabase.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Language.h"
#include "Pokemon/Pokemon_Types.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Type enum imported from Pokemon navigation.
using Pokemon::PokemonType;

//  Data attributes of a battle move.
struct MoveData{
    //  Moves whose damage doesn't come from their power (Flail, Dragon
    //  Rage) have 1; moves that do no damage 0.
    uint8_t power = 0;
    //  NONE for Curse's "???".
    PokemonType type = PokemonType::NONE;
    uint8_t pp = 0;
    //  In percent; 0 never misses.
    uint8_t accuracy = 0;
    int8_t priority = 0;
    //  pret's EFFECT_X as a slug: "hit", "flinch-hit", "sleep",
    //  "attack-down", "dragon-rage".
    std::string effect = {};
    //  The secondary effect's chance in percent (Bite's flinch: 30).
    uint8_t effect_chance = 0;
};

//  The base stats, in the game's order.
struct BaseStats{
    uint8_t hp = 0;
    uint8_t attack = 0;
    uint8_t defense = 0;
    uint8_t sp_attack = 0;
    uint8_t sp_defense = 0;
    uint8_t speed = 0;
};

//  Species battle base attributes and typing.
struct SpeciesData{
    //  The same type twice for a species with one.
    PokemonType first = PokemonType::NONE;
    PokemonType second = PokemonType::NONE;
    BaseStats base = {};
};
//  None for a move or species the data doesn't have.
const MoveData* move_data(std::string_view move);
//  Base stats and typing for species; nullptr if unknown.
const SpeciesData* species_data(std::string_view species);
//  How well `attack` hits `defend`, in tenths: 20 is 2x, 5 is 0.5x.
size_t type_effectiveness(PokemonType attack, PokemonType defend);

//  Gen 3 splits physical and special by type: Fire, Water, Grass,
//  Electric, Psychic, Ice, Dragon and Dark are special.
bool special_type(PokemonType type);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
