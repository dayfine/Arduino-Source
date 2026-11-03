/*  AutoStory Abilities
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "CommonFramework/Exceptions/ProgramFinishedException.h"
#include "PokemonFRLG_AutoStoryAbilities.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::Outcome;


namespace{

Outcome mart(Logger& logger, const MartPlan& plan, StoryAbilities& abilities){
    const MartResult result = run_mart(logger, abilities.mart, plan);
    const std::string message = mart_message(plan, result);
    const bool ok = result.end == MartEnd::DONE || result.end == MartEnd::NOTHING_TO_BUY;
    logger.log(message, result.end == MartEnd::DONE ? COLOR_BLACK : COLOR_RED);
    return ok ? Pokemon::outcome_ok() : Pokemon::outcome_failed(std::string("mart_") + mart_end_name(result.end), message);
}

Outcome teach(Logger& logger, const TeachPlan& plan, StoryAbilities& abilities){
    const TeachResult result = run_teach(logger, abilities.teach, abilities.members, plan);
    if (result.end == TeachEnd::LEARNED || result.end == TeachEnd::ALREADY_KNOWN){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed(std::string("teach_") + teach_end_name(result.end), teach_message(plan, result));
}

Outcome level(Logger& logger, const LevelCheck& check, StoryAbilities& abilities){
    const LevelResult result = run_level_check(logger, abilities.members, check);
    switch (result.end){
    case LevelEnd::REACHED:
        return Pokemon::outcome_ok();
    case LevelEnd::BELOW:
        return Pokemon::outcome_failed("level_below", std::to_string(*result.level) + " < " + std::to_string(check.level));
    case LevelEnd::UNREAD:
        break;
    }
    return Pokemon::outcome_failed("level_unread");
}

Outcome grind(Logger& logger, VideoStream* stream, const GrindPlan& plan, StoryAbilities& abilities){
    const GrindResult result = run_grind(logger, abilities.grind, abilities.members, plan);
    const std::string message = grind_message(plan, result);
    if (result.end == GrindEnd::SHINY){
        //  Not a failed Outcome, which would reload the save.
        if (stream != nullptr){
            throw ProgramFinishedException(message, *stream);
        }
        throw ProgramFinishedException(message);
    }
    if (result.end == GrindEnd::REACHED){
        return Pokemon::outcome_ok();
    }
    return Pokemon::outcome_failed(std::string("grind_") + grind_end_name(result.end), message);
}

Outcome fossil(Logger& logger, Fossil fossil, StoryAbilities& abilities){
    const ChoiceResult result = run_fossil_choice(logger, abilities.choice, fossil);
    if (result.end == ChoiceEnd::TAKEN){
        return Pokemon::outcome_ok();
    }
    logger.log("Choice: " + result.detail, COLOR_RED);
    return Pokemon::outcome_failed(std::string("fossil_") + choice_end_name(result.end), result.detail);
}

}


std::optional<Outcome> run_ability_game_step(
    Logger& logger, VideoStream* stream, const Pokemon::StoryStep& step, StoryAbilities& abilities
){
    const std::string_view variant = step.variant;
    if (variant.starts_with(MART_VARIANT_PREFIX)){
        const std::optional<MartPlan> plan = parse_mart_variant(variant);
        return plan ? mart(logger, *plan, abilities) : Pokemon::outcome_failed("bad_variant", step.variant);
    }
    if (variant.starts_with(TEACH_VARIANT_PREFIX)){
        const std::optional<TeachPlan> plan = parse_teach_variant(variant);
        return plan ? teach(logger, *plan, abilities) : Pokemon::outcome_failed("bad_variant", step.variant);
    }
    if (variant.starts_with(LEVEL_VARIANT_PREFIX)){
        const std::optional<LevelCheck> check = parse_level_variant(variant);
        return check ? level(logger, *check, abilities) : Pokemon::outcome_failed("bad_variant", step.variant);
    }
    if (variant.starts_with(GRIND_VARIANT_PREFIX)){
        const std::optional<GrindPlan> plan = parse_grind_variant(variant);
        if (!plan || !step.target){
            return Pokemon::outcome_failed("bad_variant", step.variant + (step.target ? "" : " (no nurse)"));
        }
        return grind(logger, stream, *plan, abilities);
    }
    if (variant.starts_with(FOSSIL_VARIANT_PREFIX)){
        const std::optional<Fossil> choice = parse_fossil_variant(variant);
        return choice ? fossil(logger, *choice, abilities) : Pokemon::outcome_failed("bad_variant", step.variant);
    }
    return std::nullopt;
}



}
}
}
