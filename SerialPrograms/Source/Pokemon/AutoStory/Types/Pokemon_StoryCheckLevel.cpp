/*  Story Checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <span>
#include "Pokemon_StoryCheckLevel.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string story_check_level_name(StoryCheckLevel level){
    switch (level){
    case StoryCheckLevel::TESTING:
        return "testing";
    case StoryCheckLevel::THOROUGH:
        return "thorough";
    case StoryCheckLevel::DEFAULT:
        return "default";
    case StoryCheckLevel::FAST:
        return "fast";
    case StoryCheckLevel::SPEEDRUN:
        return "speedrun";
    }
    return "?";
}


namespace{

std::span<const StoryCheckKind> story_check_kinds(){
    static constexpr StoryCheckKind KINDS[] = {
        StoryCheckKind::SAVE_PLACE,
        StoryCheckKind::START_SCREEN,
        StoryCheckKind::MENU_ROW,
        StoryCheckKind::LEG_END_TAP,
        StoryCheckKind::BAG_CONTENTS,
        StoryCheckKind::PARTY_STATS,
    };
    return KINDS;
}

std::string_view story_check_kind_name(StoryCheckKind kind){
    switch (kind){
    case StoryCheckKind::SAVE_PLACE:
        return "save place";
    case StoryCheckKind::START_SCREEN:
        return "start screen";
    case StoryCheckKind::MENU_ROW:
        return "menu row";
    case StoryCheckKind::LEG_END_TAP:
        return "leg end tap";
    case StoryCheckKind::BAG_CONTENTS:
        return "bag contents";
    case StoryCheckKind::PARTY_STATS:
        return "party stats";
    }
    return "?";
}

StoryCheckLevel lowest_check_level(StoryCheckKind kind){
    switch (kind){
    //  Cheap, and a wrong save or start wastes the whole checkpoint.
    case StoryCheckKind::SAVE_PLACE:
    case StoryCheckKind::START_SCREEN:
        return StoryCheckLevel::SPEEDRUN;
    //  A wrong row buys or uses the wrong item for the rest of the run.
    case StoryCheckKind::MENU_ROW:
    case StoryCheckKind::LEG_END_TAP:
        return StoryCheckLevel::DEFAULT;
    //  The story keeps the counts and the plan the levels: reading them
    //  again is for the user who wants everything confirmed.
    case StoryCheckKind::BAG_CONTENTS:
    case StoryCheckKind::PARTY_STATS:
        return StoryCheckLevel::THOROUGH;
    }
    return StoryCheckLevel::TESTING;
}

} // namespace

bool runs_check(StoryCheckLevel level, StoryCheckKind kind){
    //  The levels run from the most checks (TESTING) to the fewest.
    return level <= lowest_check_level(kind);
}

std::string skipped_check_line(std::string_view name, StoryCheckLevel level){
    return std::format("{}: skipped (checks: {})", name, story_check_level_name(level));
}

std::string story_checks_help(){
    std::string help;
    for (StoryCheckKind kind : story_check_kinds()){
        const StoryCheckLevel lowest = lowest_check_level(kind);
        help += std::format(
            "<br>&bull; {}: {}", story_check_kind_name(kind),
            lowest == StoryCheckLevel::SPEEDRUN ? std::string("every level") : std::format("{} and above", story_check_level_name(lowest))
        );
    }
    return help;
}




} // namespace Pokemon
} // namespace PokemonAutomation
