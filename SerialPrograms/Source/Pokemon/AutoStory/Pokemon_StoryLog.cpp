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

} // namespace


std::string event_log_line(std::string_view checkpoint, size_t number, std::string_view event, std::string_view status){
    return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
}

std::string checkpoint_start_line(std::string_view checkpoint, std::optional<size_t> events){
    if (!events){
        return std::format("[Checkpoint] {}: start", checkpoint);
    }
    return std::format("[Checkpoint] {}: start, {} events", checkpoint, *events);
}

std::string checkpoint_attempt_line(std::string_view checkpoint, size_t attempt, std::optional<size_t> attempts){
    if (!attempts){
        return std::format("[Checkpoint] {}: attempt {}", checkpoint, attempt);
    }
    return std::format("[Checkpoint] {}: attempt {}/{}", checkpoint, attempt, *attempts);
}

std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt){
    return std::format("[Checkpoint] {}: passed on attempt {} ({})", checkpoint, attempt, resets_text(attempt));
}

std::string checkpoint_reset_line(std::string_view checkpoint, size_t next_attempt){
    return std::format("[Checkpoint] {}: failed, resetting for attempt {}", checkpoint, next_attempt);
}

std::string segment_reload_line(std::string_view segment, std::string_view checkpoint, size_t next_attempt, size_t attempts){
    return std::format(
        "[Segment] {}: {} failed, reloading for attempt {}/{}",
        segment, checkpoint, next_attempt, attempts
    );
}



} // namespace Pokemon
} // namespace PokemonAutomation
