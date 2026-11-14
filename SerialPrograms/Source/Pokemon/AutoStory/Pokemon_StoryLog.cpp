/*  Story Log
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon_StoryLog.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{

std::string resets_text(size_t attempt){
    if (attempt <= 1){
        return "one-shot";
    }
    return std::format("{} reset{}", attempt - 1, attempt == 2 ? "" : "s");
}

//  "[Checkpoint] 00.04", or the game's own display text.
std::string checkpoint_prefix(std::string_view checkpoint, std::string_view display_text){
    if (!display_text.empty()){
        return std::string(display_text);
    }
    return std::format("[Checkpoint] {}", checkpoint);
}

} // namespace


std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status){
    return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
}

std::string checkpoint_start_line(
    std::string_view checkpoint, std::optional<size_t> events, std::string_view display_text
){
    const std::string prefix = checkpoint_prefix(checkpoint, display_text);
    if (!events){
        return std::format("{}: start", prefix);
    }
    return std::format("{}: start, {} events", prefix, *events);
}

std::string checkpoint_attempt_line(
    std::string_view checkpoint, size_t attempt, std::optional<size_t> attempts, std::string_view display_text
){
    const std::string prefix = checkpoint_prefix(checkpoint, display_text);
    if (!attempts){
        return std::format("{}: attempt {}", prefix, attempt);
    }
    return std::format("{}: attempt {}/{}", prefix, attempt, *attempts);
}

std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt, std::string_view display_text){
    return std::format(
        "{}: passed on attempt {} ({})", checkpoint_prefix(checkpoint, display_text), attempt, resets_text(attempt)
    );
}

std::string checkpoint_reset_line(std::string_view checkpoint, size_t next_attempt, std::string_view display_text){
    return std::format(
        "{}: failed, resetting for attempt {}", checkpoint_prefix(checkpoint, display_text), next_attempt
    );
}

std::string segment_reload_line(std::string_view segment, std::string_view checkpoint, size_t next_attempt, size_t attempts){
    return std::format(
        "[Segment] {}: {} failed, reloading for attempt {}/{}",
        segment, checkpoint, next_attempt, attempts
    );
}



} // namespace Pokemon
} // namespace PokemonAutomation
