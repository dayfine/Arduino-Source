/*  AutoStory Planned State
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A League turn as the planned policy (PokemonFRLG_AutoStoryPlannedPolicy.h)
 *  reads it: our Pokemon out and the foe out, each move with its damage
 *  rolls on the other side, and the bag. PlannedBattle builds it from the
 *  screen and the battle texts; tests build it by hand.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryPlannedState_H
#define PokemonAutomation_PokemonFRLG_AutoStoryPlannedState_H

#include <cstddef>
#include <numeric>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  A move as the policy weighs it.
struct PlannedMove{
    std::string id = {};
    int pp = 0;
    //  The chance it hits, with stages and evasion (hit_chance()).
    double accuracy = 1.0;
    //  Its damage rolls on the other side, lowest first
    //  (gen3_damage_rolls()); empty for no damage.
    std::vector<int> damage = {};
    int priority = 0;
    //  Charges a turn first (Solar Beam, Dive, Sky Attack).
    bool two_turn = false;

    bool usable() const{
        return pp > 0;
    }
    int min_damage() const{
        return damage.empty() ? 0 : damage.front();
    }
    int max_damage() const{
        return damage.empty() ? 0 : damage.back();
    }
    //  The mean roll times the chance it hits.
    double expected_hit() const{
        if (damage.empty()){
            return 0;
        }
        return static_cast<double>(std::accumulate(damage.begin(), damage.end(), 0)) / static_cast<double>(damage.size()) * accuracy;
    }
};

//  A Pokemon's status condition, as the policy tells them apart.
enum class PlannedStatus{
    NONE,
    POISON,
    PARALYSIS,
    SLEEP,
    BURN,
    FREEZE,
};

//  The item that cures `status`; none for no status.
inline std::optional<std::string_view> status_cure(PlannedStatus status){
    switch (status){
    case PlannedStatus::SLEEP:
        return "awakening";
    case PlannedStatus::POISON:
        return "antidote";
    case PlannedStatus::PARALYSIS:
        return "parlyz-heal";
    case PlannedStatus::BURN:
        return "burn-heal";
    case PlannedStatus::FREEZE:
        return "ice-heal";
    case PlannedStatus::NONE:
        return std::nullopt;
    }
    return std::nullopt;
}

//  Our Pokemon out.
struct PlannedOwn{
    int hp = 0;
    int max_hp = 0;
    PlannedStatus status = PlannedStatus::NONE;
    //  battle_speed(): with Thunder's boost, the stage and paralysis.
    int speed = 0;
    int spa_stage = 0;
    int speed_stage = 0;
    int accuracy_stage = 0;
    int attack_stage = 0;
    int defense_stage = 0;
    bool mist = false;
    bool leech_seed = false;
    //  In the move list's order.
    std::vector<PlannedMove> moves = {};
    //  The slot of a move we are locked into.
    std::optional<size_t> locked_slot = std::nullopt;
};

//  The foe out.
struct PlannedFoe{
    std::string species = {};
    //  Its trainer's party slot.
    size_t slot = 0;
    int hp = 0;
    int max_hp = 0;
    //  battle_speed(): with its stage and paralysis.
    int speed = 0;
    bool recharging = false;
    //  Asleep for more than this turn.
    bool sleeping = false;
    bool frozen = false;
    //  The two-turn move it is charging.
    std::optional<std::string> charging = std::nullopt;
    bool sun = false;
    //  Its moves, with their damage on us.
    std::vector<PlannedMove> moves = {};
};

//  One turn's state: both sides, the trainer and our bag.
struct PlannedState{
    PlannedOwn own = {};
    PlannedFoe foe = {};
    //  The trainer's Pokemon still standing, the one out included.
    int foe_party_alive = 1;
    //  The trainer has an open item slot (trainer_item_open()).
    bool trainer_item_open = false;
    SlugCounts bag = {};
};



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
