/*  AutoStory Gen 3 Damage
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FireRed/LeafGreen's damage, speed and accuracy as PokemonSimulation's
 *  League sim computes them (@smogon/calc's Gen 3 mechanics, measured raw
 *  stats): what the planned League policy's rows read (KO risk, sure KO,
 *  who moves first). Badge boosts (Boulder: Attack, Soul: Defense,
 *  Thunder: Speed, Volcano: Sp. Atk and Sp. Def) are on our side only and
 *  apply to the raw stat before its stage, rounding down.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryGen3Damage_H
#define PokemonAutomation_PokemonFRLG_AutoStoryGen3Damage_H

#include <vector>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A stat at stage `stage` (-6 to +6): (2 + stage) / 2 up, 2 / (2 - stage) down, rounded down.
int staged_stat(int stat, int stage);

//  A raw stat with its badge's 10% boost, rounded down.
int badge_boosted(int stat);

//  One hit, from the attacker's side.
struct Gen3Hit{
    int level = 1;
    //  The attacker's Attack (physical) or Sp. Atk (special), and the
    //  defender's Defense or Sp. Def: raw, with badge boosts applied.
    int attack = 1;
    int defense = 1;
    int power = 0;
    int attack_stage = 0;
    int defense_stage = 0;
    //  Same type as the attacker: x1.5.
    bool stab = false;
    //  Type effectiveness on each of the defender's types (2, 1, 0.5, 0).
    double effectiveness_first = 1.0;
    double effectiveness_second = 1.0;
    //  A critical hit: x2, and the attacker's drops and the defender's
    //  rises are ignored.
    bool critical = false;
    //  A burned attacker's physical hit: halved.
    bool burned_physical = false;
};

//  The hit's 16 damage rolls (85% to 100%), lowest first; empty for a
//  move with no power.
std::vector<int> gen3_damage_rolls(const Gen3Hit& hit);

//  The Speed a turn's order compares: `raw` (with Thunder's boost on our
//  side) at `stage`, a quarter while paralyzed.
int battle_speed(int raw, int stage, bool paralyzed);

//  The chance a move of `accuracy` percent hits,
//  with the attacker's accuracy stage and the defender's evasion stage.
double hit_chance(int accuracy, int accuracy_stage, int evasion_stage);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
