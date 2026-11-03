/*  AutoStory Damage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Gen 3 stats and damage as FireRed computes them, for the story's move
 *  choices and battle sim (PokemonFRLG_AutoStoryBattleSim.h). From
 *  pret/pokefirered:
 *  - stats: CalculateMonStats (src/pokemon.c), the nature's 10% as
 *    ModifyStatByNature;
 *  - damage: CalculateBaseDamage (src/pokemon.c), whose result already
 *    holds the +2, then in battle_script_commands.c Cmd_damagecalc's crit
 *    x2, Cmd_typecalc's STAB x1.5 and type multipliers one by one in the
 *    type chart's order (ModulateDmgByType: at least 1 unless 0x), then
 *    ApplyRandomDmgMultiplier's roll of 85 to 100 percent (at least 1).
 *    Physical or special goes by the move's type. Stat stages (and a
 *    crit's ignoring of the unhelpful ones) and the player's badge boosts
 *    are in; burn, Reflect, Light Screen, weather, abilities and held items
 *    are not.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryDamage_H
#define PokemonAutomation_PokemonFRLG_AutoStoryDamage_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include "PokemonFRLG_AutoStoryBattleData.h"

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

//  The same IV and EV in every stat, and the nature's index (0 Hardy to
//  24 Quirky; a nature raises stat n / 5 and lowers n % 5, of Attack,
//  Defense, Speed, Sp. Atk, Sp. Def).
struct Genes{
    uint8_t iv = 0;
    uint8_t ev = 0;
    uint8_t nature = 0;
};

//  A trainer's Pokemon's IVs from pret's iv (0 to 255).
uint8_t trainer_iv(uint8_t pret_iv);

Stats gen3_stats(const BaseStats& base, uint8_t level, const Genes& genes);

//  The stat stages the damage formula and the sim use, from -6 to +6.
enum class Stage : size_t{
    ATTACK,
    DEFENSE,
    SPEED,
    SP_ATTACK,
    SP_DEFENSE,
    ACCURACY,
    EVASION,
};
inline constexpr size_t STAGES = 7;
using StatStages = std::array<int8_t, STAGES>;

//  `stat` at `stage` (gStatStageRatios).
uint32_t staged_stat(uint32_t stat, int stage);

//  A Pokemon in battle as the damage formula sees it.
struct Combatant{
    PokemonType first = PokemonType::NONE;
    PokemonType second = PokemonType::NONE;
    uint8_t level = 0;
    Stats stats = {};
    StatStages stages = {};
    //  The player's badges that boost this Pokemon's stats by 10%
    //  (ShouldGetStatBadgeBoost): the Boulder Badge its Attack, the Soul
    //  Badge its Defense, the Volcano Badge its Sp. Atk and Sp. Def. Only
    //  for the player's Pokemon.
    bool boulder_badge = false;
    bool soul_badge = false;
    bool volcano_badge = false;
};

//  `species` at `level` with `genes`: its types and stats.
std::optional<Combatant> combatant(std::string_view species, uint8_t level, const Genes& genes);

//  The damage of a move whose damage doesn't come from the formula:
//  Dragon Rage 40, Sonic Boom 20, Seismic Toss and Night Shade the
//  attacker's level. None for the rest.
std::optional<uint32_t> fixed_damage(const MoveData& move, const Combatant& attacker);

//  The damage before the random roll: CalculateBaseDamage (with its +2),
//  the crit, STAB and the type chart. 0 for a move with no power, or one
//  the defender is immune to. Fixed damage as fixed_damage() says.
uint32_t damage_before_roll(const Combatant& attacker, const Combatant& defender, const MoveData& move, bool crit);

//  `before` at the random roll `percent` (85 to 100).
uint32_t rolled_damage(uint32_t before, uint32_t percent);

struct DamageRange{
    uint32_t min = 0;
    uint32_t max = 0;
};
//  The damage at the lowest roll (85) and the highest (100).
DamageRange damage_range(const Combatant& attacker, const Combatant& defender, const MoveData& move, bool crit);

//  The mean over the 16 rolls, without crits.
double average_damage(const Combatant& attacker, const Combatant& defender, const MoveData& move);



}
}
}
#endif
