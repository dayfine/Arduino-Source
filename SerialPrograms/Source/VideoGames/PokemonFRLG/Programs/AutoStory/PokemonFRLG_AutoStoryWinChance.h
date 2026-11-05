/*  AutoStory Win Chance
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Before a BattleRisk::HIGH battle: how often the lead beats the trainer,
 *  by playing the battle many times over (Monte Carlo) with the choices
 *  of PokemonFRLG_AutoStoryBattleSim.h and the game's random parts: the
 *  damage roll, crits (1/16, 1/8 for a high-crit move), accuracy with
 *  accuracy and evasion stages, speed ties, secondary effects, and the
 *  AI's ties. Too low a chance stops the story before the fight, for a
 *  GRIND or the nurse first.
 *
 *  Each turn: the Potion and the AI's item first, then the moves by
 *  priority and speed (a quarter with PARALYSIS). Sleep (2 to 5 on the
 *  game's counter), poison (1/8 HP a turn), paralysis (1/4 no move), Leech
 *  Seed (1/8 a turn to the other side), flinch, one- and two-stage stat
 *  changes, multi-hit moves (2 to 5 as the game draws them), drain (half)
 *  and Recover (half) are played; burn, freeze, confusion, recoil,
 *  recharge, two-turn moves, trapping, abilities and held items are not.
 *  The lead is never switched out: a fainted lead is a lost battle.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryWinChance_H
#define PokemonAutomation_PokemonFRLG_AutoStoryWinChance_H

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include "PokemonFRLG_AutoStoryBattleSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Outcome count and win probability estimate from Monte Carlo simulations.
struct WinEstimate{
    size_t battles = 0;
    size_t wins = 0;
    //  wins / battles
    double chance = 0;
    //  The lead and Potions the battles were played with, for the log:
    //  "wartortle Lv20 60/60 HP: bite 25/25, ...; 5 Potions".
    std::string inputs = {};
};

//  `own` with `potions` Potions against `trainer`, `battles` times, from a
//  fixed `seed` so the same inputs give the same estimate.
WinEstimate estimate_win_chance(const SimBattler& own, int potions, const SimTrainer& trainer, size_t battles = 1000, uint32_t seed = 1);

//  The story's lead (`hp_share` of its HP) with `potions` against
//  `trainer`. None if the data lacks the lead's species or the trainer.
std::optional<WinEstimate> story_win_chance(const PartyMember& lead, int potions, double hp_share, std::string_view trainer);

//  Why not to fight: "win chance 41% < 70% with <inputs>: grind first", or
//  none at `minimum` or above.
std::optional<std::string> win_chance_refusal(const WinEstimate& estimate, double minimum = MIN_WIN_CHANCE);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
