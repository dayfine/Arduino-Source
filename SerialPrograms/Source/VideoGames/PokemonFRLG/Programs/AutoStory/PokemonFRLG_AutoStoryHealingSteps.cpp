/*  AutoStory Healing Steps
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <format>
#include "Common/Cpp/Color.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryHealing.h"
#include "PokemonFRLG_AutoStoryHealingSteps.h"
#include "PokemonFRLG_AutoStoryItemsPocket.h"
#include "PokemonFRLG_AutoStoryMartPlan.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Outcome;
using Pokemon::StoryStep;

namespace{

std::string hp_text(const std::optional<int>& missing){
    return missing ? std::format("{} HP missing", *missing) : std::string("HP unknown (no bar measured since the load)");
}

Outcome heal(StoryEnv& env, const StoryStep& step, int max_hp){
    const std::optional<int> before = missing_hp(env.supplies, max_hp);
    const std::optional<int> after = heal_to_full(env.supplies, max_hp, [&](const std::string& item){
        return use_recovery_item(env.console, env.context, item, env.checks, env.language, env.supplies);
    });
    const bool full = after && *after <= 0;
    env.console.log(std::format(
        "Story: {}: {} before, {} after; {} Potions, {} Super Potions left.",
        step.name, hp_text(before), hp_text(after), env.supplies.potions, env.supplies.super_potions
    ), full ? COLOR_BLACK : COLOR_RED);
    return Pokemon::outcome_ok();
}

Outcome check(StoryEnv& env, const StoryStep& step, int max_hp){
    const std::optional<int> missing = missing_hp(env.supplies, max_hp);
    if (!missing){
        return Pokemon::outcome_failed("hp_unknown", step.name);
    }
    if (*missing > 0){
        return Pokemon::outcome_failed("hp_not_full", std::format("{}: {} HP missing", step.name, *missing));
    }
    env.console.log(std::format("Story: {}: the lead's HP is full.", step.name));
    return Pokemon::outcome_ok();
}

} // namespace


std::vector<Pokemon::StoryActionSpec> healing_action_specs(){
    const Pokemon::StoryArgSpec max_hp = required_arg("max_hp", Pokemon::StoryArgType::INTEGER, "the lead's maximum HP when its summary wasn't read");
    return {
        {.kind = Pokemon::StoryStepKind::GAME, .name = HEAL_TO_FULL_ACTION, .args = {max_hp}},
        {.kind = Pokemon::StoryStepKind::GAME, .name = CHECK_FULL_HP_ACTION, .args = {max_hp}},
    };
}

std::optional<Outcome> run_healing_step(StoryEnv& env, const StoryStep& step){
    if (step.action != HEAL_TO_FULL_ACTION && step.action != CHECK_FULL_HP_ACTION){
        return std::nullopt;
    }
    const int64_t* max_hp = game_arg<int64_t>(step.args, "max_hp");
    if (max_hp == nullptr){
        return Pokemon::outcome_failed("bad_args", step.action);
    }
    return step.action == HEAL_TO_FULL_ACTION ? heal(env, step, (int)*max_hp) : check(env, step, (int)*max_hp);
}

std::optional<Outcome> cover_hp_in_mart(Logger& logger, const Supplies& supplies, StoryStep& step){
    const int64_t* cover = step.action == MART_ACTION ? game_arg<int64_t>(step.args, "cover_hp") : nullptr;
    const std::vector<Pokemon::StoryItem>* items = game_arg<std::vector<Pokemon::StoryItem>>(step.args, "items");
    if (cover == nullptr || items == nullptr || items->empty()){
        return std::nullopt;
    }
    std::vector<Pokemon::StoryItem> lines = *items;
    const int shortfall = hp_cover_shortfall(supplies, (int)*cover, lines[0].item);
    logger.log(std::format(
        "Story: {}: {} Potions and {} Super Potions against {} HP: {} {} short.",
        step.name, supplies.potions, supplies.super_potions, *cover, shortfall, lines[0].item
    ));
    if (shortfall <= 0){
        return Pokemon::outcome_unchanged();
    }
    lines[0].quantity = std::min(lines[0].quantity, (size_t)shortfall);
    step.args["items"] = lines;
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
