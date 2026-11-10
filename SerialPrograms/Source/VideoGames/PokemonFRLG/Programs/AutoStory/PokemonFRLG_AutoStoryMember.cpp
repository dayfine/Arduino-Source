/*  AutoStory Member
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "PokemonFRLG_AutoStoryMember.h"
#include "PokemonFRLG_AutoStorySummary.h"
#include "PokemonFRLG_AutoStoryVariantWords.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


MemberReader console_member_reader(ConsoleHandle& console, ProControllerContext& context, Language language){
    return [&console, &context, language](size_t slot) -> std::optional<MemberInfo>{
        const std::optional<SummaryRead> read = read_party_summary(console, context, StartMenu::FULL, language, slot);
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


std::optional<LevelCheck> parse_level_step(const Pokemon::StoryStep& step){
    const Pokemon::StoryArgs* args = game_step_args(step, LEVEL_VARIANT, LEVEL_ARGS);
    if (args == nullptr
        || !arg_number_within(*args, "slot", 1, static_cast<int64_t>(PARTY_SLOTS))
        || !arg_number_within(*args, "min", 1, 100)
    ){
        return std::nullopt;
    }
    return LevelCheck{
        .slot = static_cast<size_t>(*Pokemon::story_arg_number(*args, "slot")) - 1,
        .level = static_cast<unsigned>(*Pokemon::story_arg_number(*args, "min")),
    };
}

Pokemon::StoryStep level_step(std::string name, std::string why, const LevelCheck& check){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = LEVEL_VARIANT;
    step.args = {
        {"slot", static_cast<int64_t>(check.slot + 1)},
        {"min", static_cast<int64_t>(check.level)},
    };
    return step;
}

LevelResult run_level_check(Logger& logger, const MemberReader& reader, const LevelCheck& check){
    const std::optional<MemberInfo> member = reader(check.slot);
    if (!member || !member->level){
        logger.log(std::format("Level: couldn't read slot {}'s level.", check.slot + 1), COLOR_RED);
        return {};
    }
    const bool reached = *member->level >= check.level;
    logger.log(std::format(
        "Level: slot {} ({}) is level {}, {} {}.",
        check.slot + 1, member->species.empty() ? "?" : member->species,
        *member->level, reached ? "at least" : "below", check.level
    ));
    return {.end = reached ? LevelEnd::REACHED : LevelEnd::BELOW, .level = member->level};
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
