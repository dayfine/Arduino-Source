/*  AutoStory League Plan
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include "PokemonFRLG_AutoStoryLeaguePlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


namespace{

//  The bag of 2026-11-16, which the user set as the least the League is
//  entered with.
struct MinimumRow{
    std::string_view item;
    int count;
};
constexpr MinimumRow LEAGUE_MINIMUM[] = {
    {"full-restore", 8},
    {"x-special", 15},
    {"x-speed", 5},
    {"x-accuracy", 3},
    {"guard-spec", 2},
    {"max-elixir", 1},
    {"elixir", 1},
    {"super-potion", 6},
};

//  The Champion keeps at least this many Full Restores (at most half the
//  bag): its fight is where the sim's chains end.
constexpr int CHAMPION_FULL_RESTORES = 3;

//  One fight's row: its Full Restore weight (the sim's median spend in
//  tenths), the setup items it wants (the note's budgeted-C2 table, X Sp /
//  X Spe / X Acc), Guard Spec and Elixirs, and its entry HP and PP.
struct FightRow{
    std::string_view trainer;
    std::string_view name;
    int full_restore_weight;
    int x_special;
    int x_speed;
    int x_accuracy;
    int guard_spec;
    int elixir;
    int entry_hp_percent;
    int surf_pp_floor;
    int blizzard_pp_floor;
};
constexpr size_t FIGHTS = 5;
constexpr std::array<FightRow, FIGHTS> LEAGUE_FIGHTS = {{
    //  Full HP from Indigo Plateau's nurse.
    {"TRAINER_ELITE_FOUR_LORELEI",       "lorelei",  6, 4, 1, 0, 0, 0, 100, 0, 0},
    //  80%: PokeGuy's 130/162; a 30% floor lost 26% of the sim's chains here.
    {"TRAINER_ELITE_FOUR_BRUNO",         "bruno",   15, 4, 5, 0, 0, 0,  80, 7, 0},
    //  27%: PokeGuy's 45/165, Torrent on.
    {"TRAINER_ELITE_FOUR_AGATHA",        "agatha",   2, 2, 1, 0, 0, 0,  27, 6, 0},
    {"TRAINER_ELITE_FOUR_LANCE",         "lance",   30, 3, 1, 1, 0, 1, 100, 6, 5},
    {"TRAINER_CHAMPION_FIRST_BULBASAUR", "champion", 22, 4, 2, 2, 1, 1, 100, 8, 2},
}};

using Split = std::array<int, FIGHTS>;

int held(const BagCounts& bag, std::string_view item){
    const auto found = bag.find(item);
    return found == bag.end() ? 0 : std::max(0, found->second);
}

//  `total` split in proportion to `weights` (largest remainder, ties to
//  the later fight); all zero if the weights are.
Split split_by_weight(int total, const Split& weights, size_t count = FIGHTS){
    Split out{};
    int sum = 0;
    for (size_t c = 0; c < count; c++){
        sum += weights[c];
    }
    if (sum <= 0 || total <= 0){
        return out;
    }
    int given = 0;
    std::array<int, FIGHTS> remainder{};
    for (size_t c = 0; c < count; c++){
        out[c] = total * weights[c] / sum;
        remainder[c] = total * weights[c] % sum;
        given += out[c];
    }
    for (; given < total; given++){
        size_t best = 0;
        for (size_t c = 0; c < count; c++){
            if (remainder[c] >= remainder[best]){
                best = c;
            }
        }
        out[best]++;
        remainder[best] = -1;
    }
    return out;
}

//  `total` against each fight's wanted count: all of it when the bag
//  covers them (the rest spare), else the Champion first and the rest in
//  proportion.
Split split_wanted(int total, const Split& wanted, int& spare){
    int sum = 0;
    for (const int count : wanted){
        sum += count;
    }
    if (total >= sum){
        spare = total - sum;
        return wanted;
    }
    spare = 0;
    const int champion = std::min(wanted[FIGHTS - 1], total);
    Split out = split_by_weight(total - champion, wanted, FIGHTS - 1);
    out[FIGHTS - 1] = champion;
    return out;
}

Split split_full_restores(int total){
    Split weights{};
    for (size_t c = 0; c < FIGHTS; c++){
        weights[c] = LEAGUE_FIGHTS[c].full_restore_weight;
    }
    Split out = split_by_weight(total, weights);
    const int reserve = std::min(CHAMPION_FULL_RESTORES, total / 2);
    for (size_t c = 0; c + 1 < FIGHTS && out[FIGHTS - 1] < reserve; c++){
        const int moved = std::min(out[c], reserve - out[FIGHTS - 1]);
        out[c] -= moved;
        out[FIGHTS - 1] += moved;
    }
    return out;
}

//  The fights' wanted counts of one item: `member` of each row.
Split wanted_of(int FightRow::* member){
    Split out{};
    for (size_t c = 0; c < FIGHTS; c++){
        out[c] = LEAGUE_FIGHTS[c].*member;
    }
    return out;
}

} // namespace


std::optional<std::string> league_shortfalls(const BagCounts& bag){
    std::string out;
    for (const MinimumRow& row : LEAGUE_MINIMUM){
        const int count = held(bag, row.item);
        if (count < row.count){
            out += std::format("{}{} {} of {}", out.empty() ? "" : ", ", row.item, count, row.count);
        }
    }
    if (out.empty()){
        return std::nullopt;
    }
    return out;
}

LeaguePlan plan_league(const BagCounts& bag){
    int spare_special = 0;
    int spare_speed = 0;
    int spare_accuracy = 0;
    int spare_guard = 0;
    int spare_elixir = 0;
    const Split full_restore = split_full_restores(held(bag, "full-restore"));
    const Split special = split_wanted(held(bag, "x-special"), wanted_of(&FightRow::x_special), spare_special);
    const Split speed = split_wanted(held(bag, "x-speed"), wanted_of(&FightRow::x_speed), spare_speed);
    const Split accuracy = split_wanted(held(bag, "x-accuracy"), wanted_of(&FightRow::x_accuracy), spare_accuracy);
    const Split guard = split_wanted(held(bag, "guard-spec"), wanted_of(&FightRow::guard_spec), spare_guard);
    const Split elixir = split_wanted(
        held(bag, "elixir") + held(bag, "max-elixir"), wanted_of(&FightRow::elixir), spare_elixir
    );
    LeaguePlan plan;
    for (size_t c = 0; c < FIGHTS; c++){
        const FightRow& row = LEAGUE_FIGHTS[c];
        plan.fights.push_back(LeagueFightPlan{
            .trainer = std::string(row.trainer),
            .name = std::string(row.name),
            .budget = {
                .full_restore = full_restore[c], .x_special = special[c], .x_speed = speed[c],
                .x_accuracy = accuracy[c], .guard_spec = guard[c], .elixir = elixir[c],
            },
            .entry_hp_percent = row.entry_hp_percent,
            .surf_pp_floor = row.surf_pp_floor,
            .blizzard_pp_floor = row.blizzard_pp_floor,
        });
    }
    plan.spare = FightBudget{
        .full_restore = 0, .x_special = spare_special, .x_speed = spare_speed,
        .x_accuracy = spare_accuracy, .guard_spec = spare_guard, .elixir = spare_elixir,
    };
    return plan;
}

std::vector<std::string> league_plan_lines(const LeaguePlan& plan){
    std::vector<std::string> lines;
    for (const LeagueFightPlan& fight : plan.fights){
        lines.push_back(std::format(
            "League plan: {}: {}; enter at {}% HP, Surf >= {}, Blizzard >= {}.",
            fight.name, budget_text(fight.budget), fight.entry_hp_percent, fight.surf_pp_floor, fight.blizzard_pp_floor
        ));
    }
    lines.push_back("League plan: spare: " + budget_text(plan.spare) + ".");
    return lines;
}

const LeagueFightPlan* league_fight(const LeaguePlan& plan, std::string_view trainer){
    for (const LeagueFightPlan& fight : plan.fights){
        if (fight.trainer == trainer){
            return &fight;
        }
    }
    return nullptr;
}




} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
