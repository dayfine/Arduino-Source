/*  AutoStory League Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include <initializer_list>
#include "Common/Cpp/Color.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "CommonFramework/Tools/DebugDumper.h"
#include "CommonFramework/VideoPipeline/VideoFeed.h"
#include "PokemonFRLG_AutoStoryBagUse.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryLeagueFightData.h"
#include "PokemonFRLG_AutoStoryLeagueSteps.h"
#include "PokemonFRLG_AutoStoryPartyMember.h"
#include "PokemonFRLG_AutoStoryPlannedBetween.h"
#include "PokemonFRLG_AutoStoryStartMenu.h"
#include "PokemonFRLG_AutoStorySummary.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Outcome;
using Pokemon::StoryStep;

namespace{

std::string joined_lines(const std::vector<std::string>& lines){
    std::string text;
    for (const std::string& line : lines){
        text += std::format("{}{}", text.empty() ? "" : ", ", line);
    }
    return text.empty() ? std::string("nothing") : text;
}

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

//  The League's items as the story counts them.
SlugCounts league_bag(const Supplies& supplies){
    SlugCounts bag;
    for (const std::string_view item : {
        "full-restore", "x-special", "x-speed", "x-accuracy", "guard-spec", "max-elixir", "elixir",
        "super-potion", "full-heal", "parlyz-heal", "antidote", "awakening", "max-potion", "hyper-potion",
    }){
        bag.emplace(std::string(item), bag_count(supplies, item));
    }
    return bag;
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

//  Our side as planned_between() reads it; none while the HP is unknown.
std::optional<PlannedCarry> top_up_carry(const Supplies& supplies, int max_hp){
    const std::optional<int> missing = missing_hp(supplies, max_hp);
    if (!missing){
        return std::nullopt;
    }
    PlannedCarry carry{.hp = max_hp - std::max(0, *missing), .max_hp = max_hp, .status = carry_status(supplies.status)};
    for (const KnownMove& known : supplies.lead.moves){
        carry.pp[known.move] = known.pp;
        carry.full_pp[known.move] = max_pp(known.move);
    }
    carry.bag = league_bag(supplies);
    return carry;
}

//  The lead's HP, when the story lost count of it since the save: read off
//  its summary (POKEMON SKILLS); else, with `full_if_unknown` (a row that
//  starts after the nurse), taken as full. False if still unknown.
bool know_lead_hp(StoryEnv& env, const StoryStep& step, int max_hp, bool full_if_unknown){
    if (missing_hp(env.supplies, max_hp)){
        return true;
    }
    const std::optional<SummaryRead> summary = read_party_summary(
        env.console, env.context, step_start_menu(step.args), env.language, /*slot=*/0
    );
    const std::optional<int> hp = summary ? summary_current_hp(summary->hp_digits, max_hp) : std::nullopt;
    if (hp){
        env.console.log(std::format("Story: {}: the summary reads HP {}/{}.", step.name, *hp, max_hp));
        measured_hp(env.supplies, static_cast<double>(*hp) / max_hp);
        return true;
    }
    if (full_if_unknown){
        env.console.log(std::format("Story: {}: HP unread; full from the nurse before this row.", step.name), COLOR_ORANGE);
        measured_hp(env.supplies, 1.0);
        return true;
    }
    return false;
}

//  The plan's top-ups before `fight` (planned_between()), one item at a
//  time from the lead's state, until none is left; the PP is read off the
//  lead's summary again after a PP item. False if an item couldn't be
//  used.
bool top_up(StoryEnv& env, const StoryStep& step, const LeagueFightData& fight, int max_hp){
    constexpr int MAX_ITEMS = 10;
    for (int c = 0; c < MAX_ITEMS; c++){
        std::optional<PlannedCarry> carry = top_up_carry(env.supplies, max_hp);
        if (!carry){
            return false;
        }
        const int hp = carry->hp;
        const std::vector<std::string> items = planned_between(*carry, fight);
        env.console.log(std::format(
            "Story: {}: before {}: HP {}/{}, {}: {}.", step.name, fight.name, hp, max_hp,
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
            env.console.log(std::format("Story: {}: couldn't use the {}.", step.name, item), COLOR_RED);
            return false;
        }
        if (item == "elixir" || item == "max-elixir"){
            read_lead_moves(env, step_start_menu(step.args));
        }
    }
    return true;
}

//  LEAGUE_TOP_UP_ACTION.
Outcome league_top_up(StoryEnv& env, const StoryStep& step){
    const std::string* name = game_arg<std::string>(step.args, "fight");
    const int64_t* fallback_max_hp = game_arg<int64_t>(step.args, "max_hp");
    const LeagueFightData* fight = name ? league_fight_data(league_plan_data(), *name) : nullptr;
    if (fight == nullptr || fallback_max_hp == nullptr){
        return Pokemon::outcome_failed("bad_args", std::format("{}: no League fight {}", step.name, name ? *name : "named"));
    }
    const bool* full_if_unknown = game_arg<bool>(step.args, "full_if_unknown");
    const int max_hp = env.supplies.lead.stats && env.supplies.lead.stats->hp > 0
        ? env.supplies.lead.stats->hp
        : static_cast<int>(*fallback_max_hp);

    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        return Pokemon::outcome_failed("precondition_failed", std::format("{}: the Items pocket wasn't read", step.name));
    }
    set_from_bag(env.supplies, rows);
    env.supplies.counts_read = true;
    //  Read right after the reload, before any item is used: the start
    //  save's bag, which the save check counts the try's spending from.
    set_from_bag(env.saved_supplies, rows);

    if (!know_lead_hp(env, step, max_hp, full_if_unknown != nullptr && *full_if_unknown)){
        return Pokemon::outcome_failed("precondition_failed", std::format("{}: the lead's HP is unknown before {}", step.name, fight->name));
    }
    if (!top_up(env, step, *fight, max_hp)){
        return Pokemon::outcome_failed("precondition_failed", std::format("{}: the top-up before {} didn't finish", step.name, fight->name));
    }
    std::optional<PlannedCarry> carry = top_up_carry(env.supplies, max_hp);
    const LeagueEntryCheck check = league_entry_check(*fight, LeagueSide{
        .hp = carry->hp, .max_hp = carry->max_hp, .pp = carry->pp, .full_pp = carry->full_pp, .bag = carry->bag,
    });
    env.console.log(std::format("Story: {}: {}'s entry contract, had/wanted: {}.", step.name, fight->name, joined_lines(check.compared)));
    if (!check.short_of.empty()){
        //  Before Lorelei this is the Indigo Plateau Center, where the Mart
        //  still sells: the shopping list, and never into the League.
        return Pokemon::outcome_failed("precondition_failed", std::format(
            "{}: {}'s entry contract is short of {}", step.name, fight->name, joined_lines(check.short_of)
        ));
    }
    return Pokemon::outcome_ok();
}

//  Fails "postcondition_unread" for `what`, keeping the screen.
Outcome unread(StoryEnv& env, std::string_view row, std::string_view what){
    if (VideoSnapshot screen = env.console.video().snapshot()){
        dump_debug_image(env.console.logger(), "AutoStory/league", "postcondition_unread", screen);
    }
    env.console.log(std::format("Story: {}: no save: {}.", row, what), COLOR_RED);
    return Pokemon::outcome_failed(std::string(Pokemon::VERIFY_UNREAD), std::format("{}: {}", row, what));
}

//  The row's top-up step's max_hp; 0 if it has none.
int row_max_hp(const StoryCheckpoint& row){
    for (const StoryStep& step : row.steps){
        if (step.action == LEAGUE_TOP_UP_ACTION){
            if (const int64_t* max_hp = game_arg<int64_t>(step.args, "max_hp")){
                return static_cast<int>(*max_hp);
            }
        }
    }
    return 0;
}

//  The lead's HP and moves' PP off its summary into `side`; the failure
//  (VERIFY_UNREAD) if a read failed or is unsure.
std::optional<Outcome> read_lead(StoryEnv& env, const StoryCheckpoint& row, const LeagueFightData& fight, LeagueSide& side){
    const int max_hp = env.supplies.lead.stats && env.supplies.lead.stats->hp > 0 ? env.supplies.lead.stats->hp : row_max_hp(row);
    const std::optional<SummaryRead> summary = read_party_summary(env.console, env.context, row.menu, env.language, /*slot=*/0);
    if (!summary){
        return unread(env, row.id, "the lead's summary wasn't read");
    }
    const std::optional<int> hp = summary_current_hp(summary->hp_digits, max_hp);
    if (!hp){
        return unread(env, row.id, std::format("the lead's HP wasn't read (\"{}\" against max {})", summary->hp_digits, max_hp));
    }
    side.hp = *hp;
    side.max_hp = max_hp;
    for (const SummaryMove& move : summary->moves){
        if (move.move.empty() || move.pp < 0){
            return unread(env, row.id, std::format("a move's PP wasn't read ({})", move.move.empty() ? "?" : move.move));
        }
        side.pp[move.move] = move.pp;
        side.full_pp[move.move] = move.max_pp > 0 ? move.max_pp : max_pp(move.move);
    }
    if (const LeagueFightData* next = next_league_fight(fight)){
        for (const auto& [move, least] : next->entry_pp){
            if (!side.pp.contains(move)){
                return unread(env, row.id, std::format("{}'s PP wasn't read", move));
            }
        }
    }
    return std::nullopt;
}

} // namespace


Outcome check_league_save(StoryEnv& env, const StoryCheckpoint& row, const Supplies& start){
    const LeagueFightData* fight = league_row_fight(row);
    if (fight == nullptr){
        return Pokemon::outcome_ok();
    }
    const std::vector<BagRow> rows = read_items_pocket(env.console, env.context, env.language);
    if (rows.empty()){
        return unread(env, row.id, "the Items pocket wasn't read");
    }
    for (const BagRow& read : rows){
        if (read.item.empty() || read.count < 0){
            return unread(env, row.id, std::format("an Items pocket row is unsure ({} x{})", read.item.empty() ? "?" : read.item, read.count));
        }
    }
    Supplies now = env.supplies;
    set_from_bag(now, rows);
    LeagueSide side{.bag = league_bag(now)};
    if (std::optional<Outcome> failed = read_lead(env, row, *fight, side)){
        return *failed;
    }
    std::string read_pp;
    for (const auto& [move, pp] : side.pp){
        read_pp += std::format(" {} {}/{}", move, pp, count_of(side.full_pp, move));
    }
    env.console.log(std::format("Story: {}: read before the save: HP {}/{}, PP{}.", row.id, side.hp, side.max_hp, read_pp));
    if (const LeagueFightData* next = next_league_fight(*fight)){
        const LeagueEntryCheck check = league_entry_check(*next, side);
        env.console.log(std::format(
            "Story: {}: {}'s entry contract after the top-up [{}], had/wanted: {}.",
            row.id, next->name, joined_lines(check.topped_up), joined_lines(check.compared)
        ));
    }
    const Outcome passed = league_row_pass(row, supplies_counts(start), side);
    if (passed.succeeded()){
        //  The story's counts are the console's from here.
        env.supplies = std::move(now);
        env.supplies.counts_read = true;
        measured_hp(env.supplies, static_cast<double>(side.hp) / side.max_hp);
    }
    return passed;
}

std::vector<Pokemon::StoryActionSpec> league_action_specs(){
    return {
        {.kind = Pokemon::StoryStepKind::GAME, .name = LEAGUE_TOP_UP_ACTION, .args = {
            required_arg("fight", Pokemon::StoryArgType::WORD, "the row's fight, as the League plan names it"),
            required_arg("max_hp", Pokemon::StoryArgType::INTEGER, "the lead's maximum HP when its summary wasn't read"),
            optional_arg("full_if_unknown", Pokemon::StoryArgType::BOOLEAN, false, "the HP is full when unread: the row starts after the nurse"),
        }},
    };
}

std::optional<Outcome> run_league_step(StoryEnv& env, const StoryStep& step){
    if (step.action != LEAGUE_TOP_UP_ACTION){
        return std::nullopt;
    }
    return league_top_up(env, step);
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
