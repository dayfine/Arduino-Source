/*  AutoStory League Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
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
#include "PokemonFRLG_AutoStoryParty.h"
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

//  A Full Restore, Elixir or Max Elixir used between fights comes out of
//  the next fight's budget (it may go below 0 here: the budget planned
//  for that fight is added to it).
void charge_next_fight(FightBudget& carried, std::string_view item){
    if (item == "full-restore"){
        carried.full_restore--;
    }else if (item == "elixir" || item == "max-elixir"){
        carried.elixir--;
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
        throw UserSetupError(env.console.logger(), std::format(
            "{}: not entering the League: the bag is short of {}. Restock to at least these counts and start this checkpoint again.",
            step.name, *short_items
        ));
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

//  The lead's state between fights, as between_fight_item() reads it, its
//  item for `next`, and the line that logs both.
std::optional<std::string> logged_between_item(
    StoryEnv& env, std::string_view step, const LeagueFightPlan& next, int hp, int max_hp
){
    const std::pair<int, int> surf = move_pp(env.supplies.lead, "surf");
    const std::pair<int, int> blizzard = move_pp(env.supplies.lead, "blizzard");
    const StatusCondition status = env.supplies.status;
    const BetweenFightView lead{
        .hp = hp,
        .max_hp = max_hp,
        .cure = status == StatusCondition::NONE ? std::string()
            : !cure_of(status).empty() ? std::string(cure_of(status)) : std::string("full-heal"),
        .surf_pp = surf.first, .surf_max_pp = surf.second,
        .blizzard_pp = blizzard.first, .blizzard_max_pp = blizzard.second,
    };
    const std::optional<std::string> item = between_fight_item(lead, league_bag(env.supplies), next);
    env.console.log(std::format(
        "Story: {}: before {}: HP {}/{}, {}, Surf {}/{}, Blizzard {}/{}: {}.",
        step, next.name, hp, max_hp, status_name(status), surf.first, surf.second,
        blizzard.first, blizzard.second, item ? std::format("the {}", *item) : std::string("ready")
    ));
    return item;
}

//  LEAGUE_BETWEEN_FIGHTS_ACTION: between_fight_item() until it has nothing
//  more, each used from the bag; a Full Restore, Elixir or Max Elixir
//  counts against the next fight's budget.
Outcome league_between_fights(StoryEnv& env, const StoryStep& step, int fallback_max_hp){
    constexpr int MAX_ITEMS = 10;
    const std::string* trainer = game_arg<std::string>(step.args, "next");
    if (trainer == nullptr || !env.supplies.league){
        return Pokemon::outcome_failed("bad_args", std::format("{}{}", step.name, env.supplies.league ? "" : ": no League plan (league_start)"));
    }
    const LeagueFightPlan* next = league_fight(env.supplies.league->plan, *trainer);
    if (next == nullptr){
        return Pokemon::outcome_failed("bad_args", std::format("{}: no League fight {}", step.name, *trainer));
    }
    const int max_hp = env.supplies.lead.stats && env.supplies.lead.stats->hp > 0 ? env.supplies.lead.stats->hp : fallback_max_hp;
    for (int c = 0; c < MAX_ITEMS; c++){
        const std::optional<int> missing = missing_hp(env.supplies, max_hp);
        std::optional<std::string> item;
        if (!missing){
            //  Never into a fight on an HP the story doesn't know (Bruno,
            //  console B, 2026-11-16: entered at 71% after "HP unknown").
            env.console.log(std::format("Story: {}: HP unknown; a Full Restore makes it known.", step.name), COLOR_RED);
            if (bag_count(env.supplies, "full-restore") > 0){
                item = "full-restore";
            }
        }else{
            item = logged_between_item(env, step.name, *next, max_hp - std::max(0, *missing), max_hp);
        }
        if (!item){
            return Pokemon::outcome_ok();
        }
        const bool used = is_lead_recovery_item(*item)
            ? use_lead_item_from_bag(env, step, *item) == ItemUseEnd::USED
            : use_recovery_item(env.console, env.context, *item, env.checks, env.language, env.supplies);
        if (!used){
            env.console.log(std::format("Story: {}: couldn't use the {}; going on.", step.name, *item), COLOR_RED);
            return Pokemon::outcome_ok();
        }
        charge_next_fight(env.supplies.league->carried, *item);
    }
    return Pokemon::outcome_ok();
}

} // namespace


std::optional<std::string> between_fight_item(
    const BetweenFightView& lead, const BagCounts& bag, const LeagueFightPlan& next
){
    //  The PP an Elixir restores to each move (pokefirered items.json).
    constexpr int ELIXIR_PP = 10;
    if (!lead.cure.empty()){
        for (const std::string_view item : {std::string_view(lead.cure), std::string_view("full-heal"), std::string_view("full-restore")}){
            if (held_count(bag, item) > 0){
                return std::string(item);
            }
        }
    }
    const int target = (lead.max_hp * next.entry_hp_percent + 99) / 100;
    if (lead.hp < target){
        if (held_count(bag, "super-potion") > 0){
            return std::string("super-potion");
        }
        if (held_count(bag, "full-restore") > 0){
            return std::string("full-restore");
        }
    }
    const bool surf_low = lead.surf_max_pp > 0 && lead.surf_pp < next.surf_pp_floor;
    const bool blizzard_low = lead.blizzard_max_pp > 0 && lead.blizzard_pp < next.blizzard_pp_floor;
    if (!surf_low && !blizzard_low){
        return std::nullopt;
    }
    const bool elixir_reaches =
        std::min(lead.surf_pp + ELIXIR_PP, lead.surf_max_pp) >= std::min(next.surf_pp_floor, lead.surf_max_pp)
        && std::min(lead.blizzard_pp + ELIXIR_PP, lead.blizzard_max_pp) >= std::min(next.blizzard_pp_floor, lead.blizzard_max_pp);
    if (elixir_reaches && held_count(bag, "elixir") > 0){
        return std::string("elixir");
    }
    if (held_count(bag, "max-elixir") > 0){
        return std::string("max-elixir");
    }
    if (held_count(bag, "elixir") > 0){
        return std::string("elixir");
    }
    return std::nullopt;
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
