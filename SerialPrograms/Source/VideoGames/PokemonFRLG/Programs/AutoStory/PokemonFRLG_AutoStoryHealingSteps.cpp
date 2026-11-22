/*  AutoStory Healing Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Color.h"
#include "PokemonFRLG_AutoStoryBag.h"
#include "PokemonFRLG_AutoStoryBagUse.h"
#include "CommonFramework/Exceptions/OperationFailedException.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_PokemonCenterFloorReader.h"
#include "VideoGames/PokemonFRLG/PokemonFRLG_Navigation.h"
#include "PokemonFRLG_AutoStoryBattlePolicy.h"
#include "PokemonFRLG_AutoStoryCenterHeal.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryLeagueSteps.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryFailure;

namespace{

//  The GAME step that uses a PP item on the lead (the header's restore_pp).
const std::string RESTORE_PP_ACTION = "restore_pp";

std::string hp_text(const std::optional<int>& missing){
    return missing ? std::format("{} HP missing", *missing) : std::string("HP unknown (no bar measured since the load)");
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

std::optional<StoryFailure> heal(StoryEnv& env, const StoryStep& step, int max_hp){
    const std::optional<int> before = missing_hp(env.supplies, max_hp);
    const int64_t* below = game_arg<int64_t>(step.args, "below_percent");
    if (below != nullptr && hp_at_least_percent(env.supplies, max_hp, static_cast<int>(*below))){
        env.console.log(std::format("Story: {}: {}, HP at least {}%: no heal.", step.name, hp_text(before), *below));
        return std::nullopt;
    }
    const auto use = [&](const std::string& item){
        if (is_lead_recovery_item(item)){
            return use_lead_item_from_bag(env, step, item) == ItemUseEnd::USED;
        }
        return use_recovery_item(env.console, env.context, item, env.checks, env.language, env.supplies);
    };
    const bool* full_restore = game_arg<bool>(step.args, "full_restore");
    const std::optional<int> after = heal_to_full(
        env.supplies, max_hp, use, full_restore != nullptr && *full_restore ? "full-restore" : ""
    );
    const bool full = after && *after <= 0;
    env.console.log(std::format(
        "Story: {}: {} before, {} after; {} Potions, {} Super Potions, {} Full Restores left.",
        step.name, hp_text(before), hp_text(after), env.supplies.potions, env.supplies.super_potions,
        bag_count(env.supplies, "full-restore")
    ), full ? COLOR_BLACK : COLOR_RED);
    return std::nullopt;
}

std::optional<StoryFailure> restore_pp_step(StoryEnv& env, const StoryStep& step){
    const std::string* item = game_arg<std::string>(step.args, "item");
    if (item == nullptr || *item != "elixir"){
        return frlg_failure("bad_args", step.action);
    }
    const ItemUseEnd end = use_lead_item_from_bag(env, step, *item);
    if (end != ItemUseEnd::USED){
        return frlg_failure(std::format("use_item_{}", item_use_end_name(end)), std::format("{}: {}", step.name, *item));
    }
    env.console.log(std::format("Story: {}: the {} on the lead.", step.name, *item));
    return std::nullopt;
}

std::optional<StoryFailure> check(StoryEnv& env, const StoryStep& step, int max_hp){
    const std::optional<int> missing = missing_hp(env.supplies, max_hp);
    if (!missing){
        return frlg_failure("hp_unknown", step.name);
    }
    if (*missing > 0){
        return frlg_failure("hp_not_full", std::format("{}: {} HP missing", step.name, *missing));
    }
    env.console.log(std::format("Story: {}: the lead's HP is full.", step.name));
    return std::nullopt;
}

std::optional<StoryFailure> bag_check(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    const bool* any = game_arg<bool>(step.args, "any");
    if (items == nullptr){
        return frlg_failure("bad_args", step.action);
    }
    const bool holds = bag_holds(env.supplies, *items, any != nullptr && *any);
    env.console.log(std::format("Story: {}: the bag {} what the step lists.", step.name, holds ? "holds" : "lacks"));
    if (holds){
        state.flags.insert(step.name);
    }
    state.changed_nothing = true;
    return std::nullopt;
}

} // namespace

//  The nurse's heal, counted only where a Center's floor shows: on 12.03
//  (console B, 2026-11-13 04:16) a route taken as walked left the player
//  on Route 21's water, the Center entry after it was taken from a still
//  screen, and the heal's A mashing was counted as the nurse's. No frame,
//  or no floor: fails with the screen.
void heal_in_center(StoryEnv& env){
    env.context.wait_for_all_requests();
    const VideoSnapshot frame = env.console.video().snapshot();
    if (!frame || !shows_pokemon_center_floor(frame)){
        OperationFailedExceptionWithScreenshot::fire(
            ErrorReportMode::SEND_ERROR_REPORT,
            "Heal: no Pokemon Center floor on screen; the player is not in a Center.",
            env.console
        );
    }
    heal_at_pokecenter(env.console, env.context);
    healed_by_nurse(env.supplies);
}


bool bag_holds(const Supplies& supplies, const std::vector<Pokemon::StoryItem>& needs, bool any){
    for (const Pokemon::StoryItem& need : needs){
        const bool held = bag_count(supplies, need.item) >= static_cast<int>(need.quantity);
        if (held == any){
            return held;
        }
    }
    return !any;
}

std::optional<std::optional<StoryFailure>> run_healing_step(StoryEnv& env, FrlgRunState& state, const StoryStep& step){
    if (step.action == BAG_CHECK_ACTION){
        return bag_check(env, state, step);
    }
    if (step.action == RESTORE_PP_ACTION){
        return restore_pp_step(env, step);
    }
    if (std::optional<std::optional<StoryFailure>> league = run_league_step(env, step)){
        return league;
    }
    if (step.action != HEAL_TO_FULL_ACTION && step.action != CHECK_FULL_HP_ACTION){
        return std::nullopt;
    }
    const int64_t* max_hp = game_arg<int64_t>(step.args, "max_hp");
    if (max_hp == nullptr){
        return frlg_failure("bad_args", step.action);
    }
    return step.action == HEAL_TO_FULL_ACTION ? heal(env, step, (int)*max_hp) : check(env, step, (int)*max_hp);
}

bool fill_mart_to(Logger& logger, const Supplies& supplies, StoryStep& step){
    const bool mart = step.action == MART_ACTION;
    const int64_t* fill = mart ? game_arg<int64_t>(step.args, "fill_to") : nullptr;
    const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    if (fill == nullptr || items == nullptr || items->empty()){
        return false;
    }
    std::vector<Pokemon::StoryItem> lines = *items;
    const int shortfall = fill_shortfall(supplies, (int)*fill, lines[0].item);
    logger.log(std::format(
        "Story: {}: {} Potions and {} Super Potions against {} {}: {} short.",
        step.name, supplies.potions, supplies.super_potions, *fill, lines[0].item, shortfall
    ));
    if (shortfall <= 0){
        return true;
    }
    lines[0].quantity = std::min(lines[0].quantity, (size_t)shortfall);
    step.args["items"] = lines;
    return false;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
