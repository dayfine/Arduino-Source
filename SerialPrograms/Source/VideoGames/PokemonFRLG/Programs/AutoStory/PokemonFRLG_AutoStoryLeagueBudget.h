/*  AutoStory League Budget
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The items one League fight may spend from the bag (its FightBudget),
 *  as PokemonFRLG_AutoStoryLeaguePlan.h plans them and the battle policy
 *  spends them.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryLeagueBudget_H
#define PokemonAutomation_PokemonFRLG_AutoStoryLeagueBudget_H

#include <format>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Item counts by bag slug ("full-restore").
using BagCounts = std::map<std::string, int, std::less<>>;

//  The items a League fight may spend from the bag.
struct FightBudget{
    int full_restore = 0;
    int x_special = 0;
    int x_speed = 0;
    int x_accuracy = 0;
    int guard_spec = 0;
    //  Elixirs and Max Elixirs.
    int elixir = 0;

    bool operator==(const FightBudget&) const = default;
};

//  `item`'s count in `budget` (the slug of a budgeted item; a Max Elixir
//  counts as an Elixir), none for an item the budget doesn't hold.
inline std::optional<int> budget_left(const FightBudget& budget, std::string_view item){
    if (item == "full-restore"){
        return budget.full_restore;
    }
    if (item == "x-special"){
        return budget.x_special;
    }
    if (item == "x-speed"){
        return budget.x_speed;
    }
    if (item == "x-accuracy"){
        return budget.x_accuracy;
    }
    if (item == "guard-spec"){
        return budget.guard_spec;
    }
    if (item == "elixir" || item == "max-elixir"){
        return budget.elixir;
    }
    return std::nullopt;
}

//  One `item` used: its budget count down, not below 0 (a KO-risk heal
//  taken over budget comes out of the bag, not the later fights' budgets).
inline void spend_budget(FightBudget& budget, std::string_view item){
    int* count = nullptr;
    if (item == "full-restore"){
        count = &budget.full_restore;
    }else if (item == "x-special"){
        count = &budget.x_special;
    }else if (item == "x-speed"){
        count = &budget.x_speed;
    }else if (item == "x-accuracy"){
        count = &budget.x_accuracy;
    }else if (item == "guard-spec"){
        count = &budget.guard_spec;
    }else if (item == "elixir" || item == "max-elixir"){
        count = &budget.elixir;
    }
    if (count != nullptr && *count > 0){
        (*count)--;
    }
}

//  `a` and `b` added up.
inline FightBudget add_budgets(const FightBudget& a, const FightBudget& b){
    return FightBudget{
        .full_restore = a.full_restore + b.full_restore,
        .x_special = a.x_special + b.x_special,
        .x_speed = a.x_speed + b.x_speed,
        .x_accuracy = a.x_accuracy + b.x_accuracy,
        .guard_spec = a.guard_spec + b.guard_spec,
        .elixir = a.elixir + b.elixir,
    };
}

//  "FR 3, X Sp 4, X Spe 2, X Acc 2, GS 1, Elixir 1".
inline std::string budget_text(const FightBudget& budget){
    return std::format(
        "FR {}, X Sp {}, X Spe {}, X Acc {}, GS {}, Elixir {}",
        budget.full_restore, budget.x_special, budget.x_speed, budget.x_accuracy, budget.guard_spec, budget.elixir
    );
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
