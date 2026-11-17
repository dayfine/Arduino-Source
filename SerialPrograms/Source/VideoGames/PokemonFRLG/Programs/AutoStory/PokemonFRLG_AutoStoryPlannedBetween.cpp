/*  AutoStory Planned Between
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  planned.py's between() and _one_heal() (PokemonSimulation #115, 837d757): keep the two
 *  in step.
 *
 */

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

constexpr std::string_view FULL_RESTORE = "full-restore";
constexpr std::string_view SUPER_POTION = "super-potion";
constexpr int SUPER_POTION_HP = 50;
//  What an Elixir restores to each move.
constexpr int ELIXIR_PP = 10;
//  The most potions the HP top-up takes.
constexpr int MAX_HP_ITEMS = 20;

//  The bag of one top-up: what may be used and what was.
class TopUp{
public:
    TopUp(PlannedCarry& carry, const LeagueFightData& next)
        : m_carry(carry)
        , m_next(next)
    {}
    bool can(std::string_view item) const{
        return spendable(m_next, m_carry.bag, item) > 0;
    }
    //  Spendable Super Potions; negative when the bag is under the reserve.
    int spare_potions() const{
        return spendable(m_next, m_carry.bag, SUPER_POTION);
    }
    void use(std::string_view item){
        m_carry.bag[std::string(item)]--;
        m_used.emplace_back(item);
    }
    std::vector<std::string> used(){
        return std::move(m_used);
    }

private:
    PlannedCarry& m_carry;
    const LeagueFightData& m_next;
    std::vector<std::string> m_used;
};

//  Cures the status; true if a Full Restore did, which healed too.
bool cure(PlannedCarry& carry, TopUp& top_up){
    if (carry.status == PlannedStatus::NONE){
        return false;
    }
    for (std::optional<std::string_view> name : {status_cure(carry.status), std::optional<std::string_view>("full-heal"), std::optional<std::string_view>(FULL_RESTORE)}){
        if (name && top_up.can(*name)){
            top_up.use(*name);
            carry.status = PlannedStatus::NONE;
            if (*name == FULL_RESTORE){
                carry.hp = carry.max_hp;
                return true;
            }
            return false;
        }
    }
    return false;
}

void heal(PlannedCarry& carry, TopUp& top_up, double share){
    const int target = static_cast<int>(carry.max_hp * share + 0.999);
    for (int c = 0; c < MAX_HP_ITEMS && carry.hp < target; c++){
        const bool potions_reach = target - carry.hp <= SUPER_POTION_HP * top_up.spare_potions();
        if (top_up.can(SUPER_POTION) && (potions_reach || !top_up.can(FULL_RESTORE))){
            top_up.use(SUPER_POTION);
            carry.hp = std::min(carry.max_hp, carry.hp + SUPER_POTION_HP);
        }else if (top_up.can(FULL_RESTORE)){
            top_up.use(FULL_RESTORE);
            carry.hp = carry.max_hp;
        }else{
            return;
        }
    }
}

//  One heal: up to 50 missing HP a Super Potion, up to 100 two (if spare),
//  more a Full Restore (two Super Potions at most without one).
void one_heal(PlannedCarry& carry, TopUp& top_up){
    const int missing = carry.max_hp - carry.hp;
    if (missing <= 0){
        return;
    }
    const int spare = top_up.spare_potions();
    int potions = 0;
    if (missing <= SUPER_POTION_HP * spare && missing <= 2 * SUPER_POTION_HP){
        potions = missing <= SUPER_POTION_HP ? 1 : 2;
    }else if (top_up.can(FULL_RESTORE)){
        top_up.use(FULL_RESTORE);
        carry.hp = carry.max_hp;
    }else{
        potions = std::min(2, std::max(0, spare));
    }
    for (int c = 0; c < potions; c++){
        top_up.use(SUPER_POTION);
        carry.hp = std::min(carry.max_hp, carry.hp + SUPER_POTION_HP);
    }
}

void restore_pp(PlannedCarry& carry, TopUp& top_up, const SlugCounts& floors){
    const bool under_floor = std::any_of(floors.begin(), floors.end(), [&](const auto& floor){
        return count_of(carry.pp, floor.first) < floor.second;
    });
    if (!under_floor){
        return;
    }
    for (std::string_view name : {"elixir", "max-elixir"}){
        if (!top_up.can(name)){
            continue;
        }
        top_up.use(name);
        for (auto& [move, pp] : carry.pp){
            const int full = count_of(carry.full_pp, move);
            pp = name == "max-elixir" ? full : std::min(full, pp + ELIXIR_PP);
        }
        return;
    }
}

} // namespace


std::vector<std::string> planned_between(PlannedCarry& carry, const LeagueFightData& next){
    TopUp top_up(carry, next);
    const bool restored = cure(carry, top_up);
    if (!next.between_one_heal){
        heal(carry, top_up, next.between_hp.value_or(1.0));
    }else if (!restored){
        one_heal(carry, top_up);
    }
    restore_pp(carry, top_up, next.between_pp);
    return top_up.used();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
