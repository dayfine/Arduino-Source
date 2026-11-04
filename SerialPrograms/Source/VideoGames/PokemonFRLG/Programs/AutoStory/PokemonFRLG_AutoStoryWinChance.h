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
 *  Each turn: the Potion, Awakening or Antidote (as the battle loop: once a
 *  battle, after PSN_TURNS_BEFORE_ANTIDOTE turns poisoned) and the AI's
 *  item first, then the moves by priority and speed (a quarter with
 *  PARALYSIS). Sleep (2 to 5 on the
 *  game's counter), poison (1/8 HP a turn), paralysis (1/4 no move), Leech
 *  Seed (1/8 a turn to the other side), flinch, one- and two-stage stat
 *  changes, multi-hit moves (2 to 5 as the game draws them), drain (half)
 *  and Recover (half) are played, and two-turn moves (two_turn()): the
 *  charge turn, PP paid then and no choice the next, cut short by a turn
 *  the user can't move; under ground or in the air every move at it but
 *  its own stat raises and heals misses. Burn, freeze, confusion, recoil,
 *  recharge, trapping, abilities and held items are not.
 *  The lead is never switched out: a fainted lead is a lost battle.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryWinChance_H
#define PokemonAutomation_PokemonFRLG_AutoStoryWinChance_H

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include "PokemonFRLG_AutoStoryBattleSim.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  How the battles were lost: the foe out when the lead fainted, and the
//  lead's state then. A battle can count under more than one state.
struct LossTally{
    std::map<std::string, size_t> by_foe = {};
    size_t seeded = 0;
    size_t poisoned = 0;
    size_t asleep = 0;
    size_t paralyzed = 0;
    //  The lead's accuracy below 0.
    size_t accuracy_down = 0;
    size_t no_potions = 0;
    //  Still going at the turn cap.
    size_t turn_cap = 0;
};

struct WinEstimate{
    size_t battles = 0;
    size_t wins = 0;
    //  wins / battles
    double chance = 0;
    //  The lead and items the battles were played with, for the log:
    //  "wartortle Lv20 60/60 HP: bite 25/25, ...; 5 Potions, 2 Awakenings".
    std::string inputs = {};
    LossTally losses = {};
};

//  The losses for the log: "lost 604: ivysaur 410, kadabra 150; seeded
//  380, poisoned 300, asleep 120, accuracy down 200, no Potions 50".
std::string loss_text(const WinEstimate& estimate);

//  `own` with `items` against `trainer`, `battles` times, from a fixed
//  `seed` so the same inputs give the same estimate.
WinEstimate estimate_win_chance(const SimBattler& own, const SimItems& items, const SimTrainer& trainer, size_t battles = 1000, uint32_t seed = 1);

//  The story's lead (`hp_share` of its HP) with `items` against
//  `trainer`. None if the data lacks the lead's species or the trainer.
std::optional<WinEstimate> story_win_chance(const PartyMember& lead, const SimItems& items, double hp_share, std::string_view trainer);

//  Why not to fight: "win chance 41% < 50% with <inputs>: grind first", or
//  none at `minimum` or above.
std::optional<std::string> win_chance_refusal(const WinEstimate& estimate, double minimum = MIN_WIN_CHANCE);



}
}
}
#endif
