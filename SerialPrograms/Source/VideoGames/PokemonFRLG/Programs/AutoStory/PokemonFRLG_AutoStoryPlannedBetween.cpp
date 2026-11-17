/*  AutoStory Planned Between
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  planned.py's between() (PokemonSimulation #111, 4a79ccc): keep the two
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

void cure(PlannedCarry& carry, TopUp& top_up){
    if (carry.status == PlannedStatus::NONE){
        return;
    }
    for (std::optional<std::string_view> name : {status_cure(carry.status), std::optional<std::string_view>("full-heal"), std::optional<std::string_view>(FULL_RESTORE)}){
        if (name && top_up.can(*name)){
            top_up.use(*name);
            carry.status = PlannedStatus::NONE;
            if (*name == FULL_RESTORE){
                carry.hp = carry.max_hp;
            }
            return;
        }
    }
}

void heal(PlannedCarry& carry, TopUp& top_up, double share){
    const int target = static_cast<int>(carry.max_hp * share + 0.999);
    for (int c = 0; c < MAX_HP_ITEMS && carry.hp < target; c++){
        const bool potions_reach = target - carry.hp <= SUPER_POTION_HP * count_of(carry.bag, "super-potion");
        if (top_up.can("super-potion") && (potions_reach || !top_up.can(FULL_RESTORE))){
            top_up.use("super-potion");
            carry.hp = std::min(carry.max_hp, carry.hp + SUPER_POTION_HP);
        }else if (top_up.can(FULL_RESTORE)){
            top_up.use(FULL_RESTORE);
            carry.hp = carry.max_hp;
        }else{
            return;
        }
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
    cure(carry, top_up);
    heal(carry, top_up, next.between_hp.value_or(1.0));
    restore_pp(carry, top_up, next.between_pp);
    return top_up.used();
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
