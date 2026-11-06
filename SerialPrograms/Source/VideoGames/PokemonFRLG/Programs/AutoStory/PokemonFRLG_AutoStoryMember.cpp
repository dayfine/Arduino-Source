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


std::string level_variant(const LevelCheck& check){
    return std::format("{}slot={} min={}", LEVEL_VARIANT_PREFIX, check.slot + 1, check.level);
}

std::optional<LevelCheck> parse_level_variant(std::string_view variant){
    if (!variant.starts_with(LEVEL_VARIANT_PREFIX)){
        return std::nullopt;
    }
    LevelCheck check;
    bool slot_seen = false;
    bool level_seen = false;
    for (std::string_view word : variant_words(variant.substr(LEVEL_VARIANT_PREFIX.size()))){
        const std::optional<VariantPair> pair = variant_pair(word);
        const std::optional<size_t> number = pair ? variant_number(pair->value, 3) : std::nullopt;
        if (!number){
            return std::nullopt;
        }
        if (pair->key == "slot" && *number >= 1 && *number <= PARTY_SLOTS){
            check.slot = *number - 1;
            slot_seen = true;
        }else if (pair->key == "min" && *number >= 1 && *number <= 100){
            check.level = (unsigned)*number;
            level_seen = true;
        }else{
            return std::nullopt;
        }
    }
    if (!slot_seen || !level_seen){
        return std::nullopt;
    }
    return check;
}

Pokemon::StoryStep level_step(std::string name, std::string why, const LevelCheck& check){
    Pokemon::StoryStep step;
    step.kind = Pokemon::StoryStepKind::GAME;
    step.name = std::move(name);
    step.why = std::move(why);
    step.variant = level_variant(check);
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
