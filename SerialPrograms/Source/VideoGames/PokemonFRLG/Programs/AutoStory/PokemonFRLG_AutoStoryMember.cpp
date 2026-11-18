/*  AutoStory Member
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon/AutoStory/Types/Pokemon_StoryLevelCheck.h"
#include "PokemonFRLG_AutoStoryGameArgs.h"
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStorySummary.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


MemberReader console_member_reader(ConsoleHandle& console, ProControllerContext& context, Language language, StartMenu menu){
    return [&console, &context, language, menu](size_t slot) -> std::optional<MemberInfo>{
        const std::optional<SummaryRead> read = read_party_summary(console, context, menu, language, slot);
        if (!read || read->species.empty() || !read->level || read->moves.empty()){
            console.log(std::format("Party: couldn't read the member in slot {}.", slot + 1), COLOR_RED);
            return std::nullopt;
        }
        MemberInfo info{.species = read->species, .level = read->level, .moves = {}, .stats = read->stats};
        for (const SummaryMove& move : read->moves){
            info.moves.push_back(move.move);
        }
        return info;
    };
}


Pokemon::StoryActionArgs level_args(const LevelCheck& check){
    return {{"slot", int64_t(check.slot + 1)}, {"min", int64_t(check.level)}};
}

std::optional<LevelCheck> level_check_of(const Pokemon::StoryActionArgs& args){
    const std::optional<size_t> slot = counted_arg(args, "slot", PARTY_SLOTS);
    const std::optional<size_t> level = number_arg(args, "min", 1, 100);
    if (!slot || !level){
        return std::nullopt;
    }
    return LevelCheck{.slot = *slot, .level = (unsigned)*level};
}

Pokemon::StoryStep level_step(std::string name, std::string why, const LevelCheck& check){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.action = LEVEL_ACTION;
    step.args = level_args(check);
    return step;
}

LevelResult run_level_check(Logger& logger, const MemberReader& reader, const LevelCheck& check){
    const std::optional<MemberInfo> member = reader(check.slot);
    if (!member || !member->level){
        logger.log(std::format("Level: couldn't read slot {}'s level.", check.slot + 1), COLOR_RED);
        return {};
    }
    const Pokemon::LevelVerdict verdict = Pokemon::level_verdict(*member->level, check.level);
    const std::string species = member->species.empty() ? "?" : member->species;
    switch (verdict){
    case Pokemon::LevelVerdict::MET:
        logger.log(std::format("Level: slot {} ({}) is level {}, at least {}.", check.slot + 1, species, *member->level, check.level));
        break;
    case Pokemon::LevelVerdict::WITHIN_TOLERANCE:
        logger.log(std::format(
            "Level: slot {} ({}) is level {}, below {} but within {} of it: going on.",
            check.slot + 1, species, *member->level, check.level, Pokemon::LEVEL_CHECK_TOLERANCE
        ), COLOR_ORANGE);
        break;
    case Pokemon::LevelVerdict::BELOW:
        logger.log(std::format(
            "Level: slot {} ({}) is level {}, more than {} below {}.",
            check.slot + 1, species, *member->level, Pokemon::LEVEL_CHECK_TOLERANCE, check.level
        ), COLOR_RED);
        break;
    }
    return {.end = verdict == Pokemon::LevelVerdict::BELOW ? LevelEnd::BELOW : LevelEnd::REACHED, .level = member->level};
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
