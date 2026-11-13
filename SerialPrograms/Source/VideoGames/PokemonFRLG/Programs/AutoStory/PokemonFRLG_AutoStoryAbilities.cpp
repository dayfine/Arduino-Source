/*  AutoStory Abilities
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
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
    logger.log(message, result.end == MartEnd::DONE ? COLOR_BLACK : COLOR_RED);
    return result.end == MartEnd::DONE ? Pokemon::outcome_ok() : Pokemon::outcome_failed(std::format("mart_{}", mart_end_name(result.end)), message);
}

Outcome teach(Logger& logger, const TeachPlan& plan, StoryAbilities& abilities){
    const TeachResult result = run_teach(logger, abilities.teach, abilities.members, plan, abilities.species);
    if ((result.end == TeachEnd::LEARNED || result.end == TeachEnd::ALREADY_KNOWN) && abilities.party != nullptr && is_field_move(plan.move)){
        const std::string& species = result.species.empty() ? plan.species : result.species;
        logger.log(std::format("Party: {} is used by slot {} ({}).", plan.move, result.member + 1, species.empty() ? "species unread" : species));
        abilities.party->users().record(plan.move, result.member, species);
    }
    if (result.end == TeachEnd::LEARNED){
        return Pokemon::outcome_ok();
    }
    //  NO_ITEM: skipped, with run_teach()'s log line.
    if (result.end == TeachEnd::ALREADY_KNOWN || result.end == TeachEnd::NO_ITEM){
        return Pokemon::outcome_unchanged();
    }
    return Pokemon::outcome_failed(std::format("teach_{}", teach_end_name(result.end)), teach_message(plan, result));
}

Outcome level(Logger& logger, const LevelCheck& check, StoryAbilities& abilities){
    const LevelResult result = run_level_check(logger, abilities.members, check);
    switch (result.end){
    case LevelEnd::REACHED:
        return Pokemon::outcome_unchanged();
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
    return Pokemon::outcome_failed(std::string("grind_") + std::string(grind_end_name(result.end)), message);
}

Outcome fossil(Logger& logger, Fossil fossil, StoryAbilities& abilities){
    const ChoiceResult result = run_fossil_choice(logger, abilities.choice, fossil);
    if (result.end == ChoiceEnd::TAKEN){
        return Pokemon::outcome_ok();
    }
    logger.log("Choice: " + result.detail, COLOR_RED);
    return Pokemon::outcome_failed(std::string("fossil_") + std::string(choice_end_name(result.end)), result.detail);
}

} // namespace


std::optional<Outcome> run_ability_game_step(
    Logger& logger, VideoStream* stream, const Pokemon::StoryStep& step, StoryAbilities& abilities
){
    if (step.action == MART_ACTION){
        const std::optional<MartPlan> plan = mart_plan_of(step.args);
        return plan ? mart(logger, *plan, abilities) : Pokemon::outcome_failed("bad_args", step.action);
    }
    if (step.action == TEACH_ACTION){
        const std::optional<TeachPlan> plan = teach_plan_of(step.args);
        return plan ? teach(logger, *plan, abilities) : Pokemon::outcome_failed("bad_args", step.action);
    }
    if (step.action == LEVEL_ACTION){
        const std::optional<LevelCheck> check = level_check_of(step.args);
        return check ? level(logger, *check, abilities) : Pokemon::outcome_failed("bad_args", step.action);
    }
    if (step.action == GRIND_ACTION){
        const std::optional<GrindPlan> plan = grind_plan_of(step.args);
        if (!plan || !step.args.contains("nurse")){
            return Pokemon::outcome_failed("bad_args", step.action + (step.args.contains("nurse") ? "" : " (no nurse)"));
        }
        return grind(logger, stream, *plan, abilities);
    }
    if (step.action == FOSSIL_ACTION){
        const std::optional<Fossil> choice = fossil_of(step.args);
        return choice ? fossil(logger, *choice, abilities) : Pokemon::outcome_failed("bad_args", step.action);
    }
    return std::nullopt;
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
