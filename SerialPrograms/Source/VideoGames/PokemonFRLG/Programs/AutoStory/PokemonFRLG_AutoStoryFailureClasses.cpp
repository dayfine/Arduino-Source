/*  AutoStory Failure Classes
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "PokemonFRLG_AutoStoryFailureClasses.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{

using Pokemon::StoryFailureClass;


namespace{

using C = StoryFailureClass;

//  FRLG's own failure names, by the step that fails with them.
const std::map<std::string_view, StoryFailureClass, std::less<>>& frlg_failure_names(){
    static const std::map<std::string_view, StoryFailureClass, std::less<>> names{
        //  The row's data: args, a target, legs or an item the step can't use.
        {"bad_args", C::STORY_ERROR},
        {"bad_target", C::STORY_ERROR},
        {"bad_legs", C::STORY_ERROR},
        {"unknown_item", C::STORY_ERROR},
        //  A reload with nothing saved yet.
        {"no_save", C::RELOAD_FAILED},
        //  Where the player is.
        {"no_pose", C::POSE_LOST},
        {"unknown_facing", C::POSE_LOST},
        {"no_route", C::POSE_LOST},
        {"step_unresolved", C::POSE_LOST},
        //  A walk that makes no headway.
        {"route_move_budget_exceeded", C::BLOCKED},
        {"native_route_stopped", C::BLOCKED},
        //  A warp the replay could not confirm.
        {"transition_unconfirmed", C::LATE_FADE},
        //  A screen the step waits for, not seen.
        {"no_trainer", C::SCREEN_UNREAD},
        {"no_battle", C::SCREEN_UNREAD},
        {"no_message", C::SCREEN_UNREAD},
        {"level_unread", C::SCREEN_UNREAD},
        {"hp_unknown", C::SCREEN_UNREAD},
        {"count_unread", C::SCREEN_UNREAD},
        {"scene_evidence_missing_or_mismatched", C::SCREEN_UNREAD},
        //  The game's state, read, is not what the row needs.
        {"level_below", C::STATE_CHECK_FAILED},
        {"hp_not_full", C::STATE_CHECK_FAILED},
        {"item_not_found", C::STATE_CHECK_FAILED},
        //  Menus.
        {"pc_refused", C::MENU_FAILED},
        {"pc_failed", C::MENU_FAILED},
        //  Battles and what interrupts a step.
        {"blackout", C::BATTLE_LOST},
        {"dialog", C::INTERRUPT_FAILED},
        {"encounter_requires_recovery", C::WILD_BATTLES},
        //  The row sim's own checks (sim runs only).
        {"sim_failure", C::SIM_MISMATCH},
        {"wrong_end", C::SIM_MISMATCH},
        {"wall_overrun", C::SIM_MISMATCH},
        {"mart_cancel", C::SIM_MISMATCH},
        {"wrong_item", C::SIM_MISMATCH},
        //  An exception out of a step, as the event line names it.
        {"OperationFailedException", C::OPERATION_FAILED},
    };
    return names;
}

//  The families of names a step makes from its own end ("mart_" +
//  mart_end_name()), by prefix.
const std::vector<std::pair<std::string_view, StoryFailureClass>>& frlg_failure_prefixes(){
    static const std::vector<std::pair<std::string_view, StoryFailureClass>> prefixes{
        {"mart_", C::MENU_FAILED},
        {"teach_", C::MENU_FAILED},
        {"use_item_", C::MENU_FAILED},
        {"fossil_", C::MENU_FAILED},
        {"fly_", C::MENU_FAILED},
        {"capture_", C::OPERATION_FAILED},
        {"grind_", C::OPERATION_FAILED},
        {"trash_cans_", C::OPERATION_FAILED},
        {"slope_", C::WALK_SHORT},
    };
    return prefixes;
}

bool is_name_char(char c){
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

//  The failure name at the start of `text`, up to a ':' or the end; none
//  when `text` starts with anything else.
std::optional<std::string_view> leading_name(std::string_view text){
    size_t end = 0;
    while (end < text.size() && is_name_char(text[end])){
        end++;
    }
    if (end == 0 || (end < text.size() && text[end] != ':')){
        return std::nullopt;
    }
    return text.substr(0, end);
}

//  The failure name of a line with the word "failed", then ": <name>" or
//  " at <where>: <name>".
std::optional<std::string_view> failed_line_name(std::string_view line){
    constexpr std::string_view FAILED = " failed";
    const size_t at = line.find(FAILED);
    if (at == std::string_view::npos){
        return std::nullopt;
    }
    std::string_view rest = line.substr(at + FAILED.size());
    if (rest.starts_with(" at ")){
        const size_t colon = rest.find(": ");
        if (colon == std::string_view::npos){
            return std::nullopt;
        }
        rest = rest.substr(colon);
    }
    if (!rest.starts_with(": ")){
        return std::nullopt;
    }
    return leading_name(rest.substr(2));
}

//  The failure name of a "[Story] <name>: <detail>" line.
std::optional<std::string_view> story_line_name(std::string_view line){
    constexpr std::string_view STORY = "[Story] ";
    if (!line.starts_with(STORY)){
        return std::nullopt;
    }
    return leading_name(line.substr(STORY.size()));
}

} // namespace


std::optional<StoryFailureClass> frlg_failure_class(std::string_view failure){
    const auto& names = frlg_failure_names();
    if (const auto it = names.find(failure); it != names.end()){
        return it->second;
    }
    for (const auto& [prefix, failure_class] : frlg_failure_prefixes()){
        if (failure.size() > prefix.size() && failure.starts_with(prefix)){
            return failure_class;
        }
    }
    return Pokemon::story_failure_class_of_outcome(failure);
}

std::string tag_failure_line(std::string_view line){
    if (Pokemon::read_story_failure_class_tag(line)){
        return std::string(line);
    }
    std::optional<std::string_view> failure = failed_line_name(line);
    if (!failure){
        failure = story_line_name(line);
    }
    const std::optional<StoryFailureClass> failure_class = failure ? frlg_failure_class(*failure) : std::nullopt;
    if (!failure_class){
        return std::string(line);
    }
    return std::format("{} {}", line, Pokemon::story_failure_class_tag(*failure_class));
}



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
