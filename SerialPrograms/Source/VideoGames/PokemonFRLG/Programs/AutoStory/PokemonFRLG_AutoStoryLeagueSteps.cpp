/*  AutoStory League Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <initializer_list>
#include <utility>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Exceptions.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryBagUse.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryLeaguePlan.h"
#include "PokemonFRLG_AutoStoryLeagueSteps.h"
#include "PokemonFRLG_AutoStoryEvents.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeaguePlanData.h"
#include "PokemonFRLG_AutoStoryParty.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Outcome;
using Pokemon::StoryStep;

namespace{

//  `item` (is_lead_recovery_item()) on the lead from the bag, found by
//  reading the pocket: taken into the supplies if used.
ItemUseEnd use_lead_item_from_bag(StoryEnv& env, const StoryStep& step, const std::string& item){
    ConsoleItemUseActions bag(env.console, env.context, env.language, step_start_menu(step.args));
    const ItemUseEnd end = use_item_from_bag(env.console.logger(), bag, item);
    if (end == ItemUseEnd::USED){
        took_recovery_item(env.supplies, item);
    }
    return end;
}

int held_count(const BagCounts& bag, std::string_view item){
    const auto found = bag.find(item);
    return found == bag.end() ? 0 : std::max(0, found->second);
}

//  A budgeted item (a Full Restore, Hyper or Max Potion, Full Heal, Elixir
//  or Max Elixir) used between fights comes out of the next fight's
//  budget (it may go below 0 here: the budget planned for that fight is
//  added to it).
void charge_next_fight(FightBudget& carried, std::string_view item){
    if (int* count = budget_count(item, carried)){
        (*count)--;
    }
}

//  The League's items as the story counts them.
BagCounts league_bag(const Supplies& supplies){
    BagCounts bag;
    for (const std::string_view item : {
        "full-restore", "x-special", "x-speed", "x-accuracy", "guard-spec", "max-elixir", "elixir",
        "super-potion", "full-heal", "parlyz-heal", "antidote", "awakening", "max-potion", "hyper-potion",
    }){
        bag.emplace(std::string(item), bag_count(supplies, item));
    }
    return bag;
}

//  LEAGUE_START_ACTION: the Items pocket read, checked against the
//  League's minimum and planned.
Outcome league_start(StoryEnv& env, const StoryStep& step){
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        return Pokemon::outcome_failed("bag_unread", step.name);
    }
    set_from_bag(env.supplies, rows);
    env.supplies.counts_read = true;
    const BagCounts bag = league_bag(env.supplies);
    if (const std::optional<std::string> short_items = league_shortfalls(bag)){
        env.console.log(std::format(
            "Story: {}: the bag is short of the League's restock list: {}. Planning from the bag as it is.",
            step.name, *short_items
        ), COLOR_ORANGE);
    }
    LeagueRun run{.plan = plan_league(bag), .carried = {}};
    for (const std::string& line : league_plan_lines(run.plan)){
        env.console.log(line);
    }
    env.supplies.league = std::move(run);
    //  The row starts after Indigo Plateau's nurse (20.01a) and nothing
    //  hurts the lead before Lorelei; a fresh program has measured nothing.
    if (!missing_hp(env.supplies, 1)){
        env.console.log(std::format("Story: {}: HP unknown since the load; full from the nurse before this row.", step.name), COLOR_ORANGE);
        measured_hp(env.supplies, 1.0);
    }
    return Pokemon::outcome_ok();
}

//  The lead's move `move`: its PP and full PP, 0 and 0 if it doesn't know it.
std::pair<int, int> move_pp(const PartyMember& lead, std::string_view move){
    for (const KnownMove& known : lead.moves){
        if (known.move == move){
            return {known.pp, max_pp(move)};
        }
    }
    return {0, 0};
}

//  The lead's state between fights, as between_fight_action() reads it
//  (`hp_reread`: READ_HP was done this step), its move for `next`, and the
//  line that logs both.
BetweenFightAction logged_between_action(
    StoryEnv& env, std::string_view step, const LeagueFightPlan& next, int max_hp, bool hp_reread
){
    const std::pair<int, int> surf = move_pp(env.supplies.lead, "surf");
    const std::pair<int, int> blizzard = move_pp(env.supplies.lead, "blizzard");
    const StatusCondition status = env.supplies.status;
    const std::optional<int> missing = missing_hp(env.supplies, max_hp);
    const BetweenFightView lead{
        .hp = missing ? std::optional<int>(max_hp - std::max(0, *missing)) : std::nullopt,
        .max_hp = max_hp,
        .hp_reread = hp_reread,
        .cure = status == StatusCondition::NONE ? std::string()
            : !cure_of(status).empty() ? std::string(cure_of(status)) : std::string("full-heal"),
        .surf_pp = surf.first, .surf_max_pp = surf.second,
        .blizzard_pp = blizzard.first, .blizzard_max_pp = blizzard.second,
    };
    const BetweenFightAction action = between_fight_action(lead, league_bag(env.supplies), next);
    const std::string what = action.move == BetweenFightMove::USE_ITEM ? std::format("the {}", action.item.value_or("?"))
        : action.move == BetweenFightMove::READ_HP ? std::string("read the HP again")
        : action.move == BetweenFightMove::HP_UNKNOWN ? std::string("HP still unknown")
        : std::string("ready");
    env.console.log(std::format(
        "Story: {}: before {}: HP {}/{}, {}, Surf {}/{}, Blizzard {}/{}: {}.",
        step, next.name, lead.hp ? std::to_string(*lead.hp) : std::string("?"), max_hp, status_name(status),
        surf.first, surf.second, blizzard.first, blizzard.second, what
    ), action.move == BetweenFightMove::READY || action.move == BetweenFightMove::USE_ITEM ? COLOR_BLACK : COLOR_RED);
    return action;
}

PlannedStatus carry_status(StatusCondition status){
    switch (status){
    case StatusCondition::POISON:
        return PlannedStatus::POISON;
    case StatusCondition::PARALYSIS:
        return PlannedStatus::PARALYSIS;
    case StatusCondition::SLEEP:
        return PlannedStatus::SLEEP;
    case StatusCondition::OTHER:
        return PlannedStatus::BURN;
    case StatusCondition::NONE:
        break;
    }
    return PlannedStatus::NONE;
}

//  Our side between fights as planned_between() reads it; none while the
//  HP is unknown.
std::optional<PlannedCarry> between_carry(const Supplies& supplies, int max_hp){
    const std::optional<int> missing = missing_hp(supplies, max_hp);
    if (!missing){
        return std::nullopt;
    }
    PlannedCarry carry{.hp = max_hp - std::max(0, *missing), .max_hp = max_hp, .status = carry_status(supplies.status)};
    for (const KnownMove& known : supplies.lead.moves){
        carry.pp[known.move] = known.pp;
        carry.full_pp[known.move] = max_pp(known.move);
    }
    for (const auto& [item, count] : league_bag(supplies)){
        carry.bag[item] = count;
    }
    return carry;
}

//  The planned policy's top-ups before `next` (planned_between()), one
//  item at a time from the lead's state, until none is left; the PP is
//  read off the lead's summary again after a PP item. False if the HP is
//  unknown.
bool planned_between_fights(StoryEnv& env, const StoryStep& step, const LeagueFightData& next, int max_hp){
    constexpr int MAX_ITEMS = 10;
    for (int c = 0; c < MAX_ITEMS; c++){
        std::optional<PlannedCarry> carry = between_carry(env.supplies, max_hp);
        if (!carry){
            return false;
        }
        const int hp = carry->hp;
        const std::vector<std::string> items = planned_between(*carry, next);
        env.console.log(std::format(
            "Story: {}: before {}: HP {}/{}, {}: {}.", step.name, next.name, hp, max_hp,
            status_name(env.supplies.status), items.empty() ? std::string("ready") : std::format("the {}", items.front())
        ));
        if (items.empty()){
            return true;
        }
        const std::string& item = items.front();
        const bool used = is_lead_recovery_item(item)
            ? use_lead_item_from_bag(env, step, item) == ItemUseEnd::USED
            : use_recovery_item(env.console, env.context, item, env.checks, env.language, env.supplies);
        if (!used){
            env.console.log(std::format("Story: {}: couldn't use the {}; going on.", step.name, item), COLOR_RED);
            return true;
        }
        if (env.supplies.league){
            charge_next_fight(env.supplies.league->carried, item);
        }
        if (item == "elixir" || item == "max-elixir"){
            read_lead_moves(env, step_start_menu(step.args));
        }
    }
    return true;
}

//  LEAGUE_BETWEEN_FIGHTS_ACTION: between_fight_action()'s moves until it is
//  READY, each item used from the bag; a Full Restore, Elixir or Max Elixir
//  counts against the next fight's budget. READ_HP: no overworld reader
//  has the lead's current HP yet (the summary's POKEMON SKILLS read keeps
//  only its maximum), so the HP stays unknown and the step fails before
//  the fight.
Outcome league_between_fights(StoryEnv& env, const StoryStep& step, int fallback_max_hp){
    constexpr int MAX_MOVES = 10;
    const std::string* trainer = game_arg<std::string>(step.args, "next");
    if (trainer == nullptr || !env.supplies.league){
        return Pokemon::outcome_failed("bad_args", std::format("{}{}", step.name, env.supplies.league ? "" : ": no League plan (league_start)"));
    }
    const LeagueFightPlan* next = league_fight(env.supplies.league->plan, *trainer);
    if (next == nullptr){
        return Pokemon::outcome_failed("bad_args", std::format("{}: no League fight {}", step.name, *trainer));
    }
    const int max_hp = env.supplies.lead.stats && env.supplies.lead.stats->hp > 0 ? env.supplies.lead.stats->hp : fallback_max_hp;
    //  The planned policy's targets (frlg-league-plan.json's between rows).
    const std::optional<std::string> fight = league_fight_name(*trainer);
    if (const LeagueFightData* planned = fight ? league_fight_data(league_plan_data(), *fight) : nullptr){
        return planned_between_fights(env, step, *planned, max_hp)
            ? Pokemon::outcome_ok()
            : Pokemon::outcome_failed("hp_unknown", std::format("{}: before {}", step.name, planned->name));
    }
    bool hp_reread = false;
    for (int c = 0; c < MAX_MOVES; c++){
        const BetweenFightAction action = logged_between_action(env, step.name, *next, max_hp, hp_reread);
        switch (action.move){
        case BetweenFightMove::READY:
            return Pokemon::outcome_ok();
        case BetweenFightMove::READ_HP:
            hp_reread = true;
            continue;
        case BetweenFightMove::HP_UNKNOWN:
            //  Never into a fight on an HP the story doesn't know (Bruno,
            //  console B, 2026-11-16: entered at 71% after "HP unknown").
            return Pokemon::outcome_failed("hp_unknown", std::format("{}: before {}", step.name, next->name));
        case BetweenFightMove::USE_ITEM:
            break;
        }
        const std::string item = action.item.value_or("");
        const bool used = is_lead_recovery_item(item)
            ? use_lead_item_from_bag(env, step, item) == ItemUseEnd::USED
            : use_recovery_item(env.console, env.context, item, env.checks, env.language, env.supplies);
        if (!used){
            env.console.log(std::format("Story: {}: couldn't use the {}; going on.", step.name, item), COLOR_RED);
            return Pokemon::outcome_ok();
        }
        charge_next_fight(env.supplies.league->carried, item);
    }
    return Pokemon::outcome_ok();
}

} // namespace


BetweenFightAction between_fight_action(
    const BetweenFightView& lead, const BagCounts& bag, const LeagueFightPlan& next
){
    //  The PP an Elixir restores to each move (pokefirered items.json).
    constexpr int ELIXIR_PP = 10;
    const auto first_held = [&](std::initializer_list<std::string_view> items) -> std::optional<BetweenFightAction>{
        for (const std::string_view item : items){
            if (held_count(bag, item) > 0){
                return BetweenFightAction{.move = BetweenFightMove::USE_ITEM, .item = std::string(item)};
            }
        }
        return std::nullopt;
    };
    if (!lead.cure.empty()){
        if (std::optional<BetweenFightAction> cure = first_held({lead.cure, "full-heal", "full-restore"})){
            return *cure;
        }
    }
    if (!lead.hp){
        return BetweenFightAction{.move = lead.hp_reread ? BetweenFightMove::HP_UNKNOWN : BetweenFightMove::READ_HP};
    }
    const int target = (lead.max_hp * next.entry_hp_percent + 99) / 100;
    if (*lead.hp < target){
        if (std::optional<BetweenFightAction> heal = first_held({"super-potion", "hyper-potion", "max-potion", "full-restore"})){
            return *heal;
        }
    }
    const bool surf_low = lead.surf_max_pp > 0 && lead.surf_pp < next.surf_pp_floor;
    const bool blizzard_low = lead.blizzard_max_pp > 0 && lead.blizzard_pp < next.blizzard_pp_floor;
    if (!surf_low && !blizzard_low){
        return BetweenFightAction{};
    }
    const bool elixir_reaches =
        std::min(lead.surf_pp + ELIXIR_PP, lead.surf_max_pp) >= std::min(next.surf_pp_floor, lead.surf_max_pp)
        && std::min(lead.blizzard_pp + ELIXIR_PP, lead.blizzard_max_pp) >= std::min(next.blizzard_pp_floor, lead.blizzard_max_pp);
    const std::optional<BetweenFightAction> pp = elixir_reaches
        ? first_held({"elixir", "max-elixir"})
        : first_held({"max-elixir", "elixir"});
    return pp.value_or(BetweenFightAction{});
}


std::vector<Pokemon::StoryActionSpec> league_action_specs(){
    return {
        {.kind = Pokemon::StoryStepKind::GAME, .name = LEAGUE_START_ACTION, .args = {}},
        {.kind = Pokemon::StoryStepKind::GAME, .name = LEAGUE_BETWEEN_FIGHTS_ACTION, .args = {
            required_arg("max_hp", Pokemon::StoryArgType::INTEGER, "the lead's maximum HP when its summary wasn't read"),
            required_arg("next", Pokemon::StoryArgType::WORD, "the next fight's trainer, as its battle step names it"),
        }},
    };
}

std::optional<Outcome> run_league_step(StoryEnv& env, const StoryStep& step){
    if (step.action == LEAGUE_START_ACTION){
        return league_start(env, step);
    }
    if (step.action != LEAGUE_BETWEEN_FIGHTS_ACTION){
        return std::nullopt;
    }
    const int64_t* max_hp = game_arg<int64_t>(step.args, "max_hp");
    if (max_hp == nullptr){
        return Pokemon::outcome_failed("bad_args", step.action);
    }
    return league_between_fights(env, step, static_cast<int>(*max_hp));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
