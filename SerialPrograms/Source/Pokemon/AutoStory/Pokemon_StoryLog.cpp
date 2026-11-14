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

//  "[Checkpoint] 00.04", or the game's own checkpoint text unchanged.
std::string checkpoint_prefix(std::string_view checkpoint, bool add_checkpoint_prefix){
    if (!add_checkpoint_prefix){
        return std::string(checkpoint);
    }
    return std::format("[Checkpoint] {}", checkpoint);
}

} // namespace


std::string checkpoint_start_line(
    std::string_view checkpoint, std::optional<size_t> events, bool add_checkpoint_prefix
){
    const std::string prefix = checkpoint_prefix(checkpoint, add_checkpoint_prefix);
    if (!events){
        return std::format("{}: start", prefix);
    }
    return std::format("{}: start, {} events", prefix, *events);
}

std::string checkpoint_attempt_line(
    std::string_view checkpoint, size_t attempt, std::optional<size_t> attempts, bool add_checkpoint_prefix
){
    const std::string prefix = checkpoint_prefix(checkpoint, add_checkpoint_prefix);
    if (!attempts){
        return std::format("{}: attempt {}", prefix, attempt);
    }
    return std::format("{}: attempt {}/{}", prefix, attempt, *attempts);
}

std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt, bool add_checkpoint_prefix){
    return std::format(
        "{}: passed on attempt {} ({})",
        checkpoint_prefix(checkpoint, add_checkpoint_prefix), attempt, resets_text(attempt)
    );
}

std::string checkpoint_reset_line(
    std::string_view checkpoint, size_t next_attempt, std::optional<size_t> attempts, bool add_checkpoint_prefix
){
    const std::string prefix = checkpoint_prefix(checkpoint, add_checkpoint_prefix);
    if (!attempts){
        return std::format("{}: failed, resetting for attempt {}", prefix, next_attempt);
    }
    return std::format("{}: failed, resetting for attempt {}/{}", prefix, next_attempt, *attempts);
}

std::string story_short_line(std::string_view line){
    if (!line.starts_with('[')){
        return std::string(line);
    }
    const size_t end = line.find("] ");
    if (end == std::string_view::npos){
        return std::string(line);
    }
    return std::string(line.substr(end + 2));
}



} // namespace Pokemon
} // namespace PokemonAutomation
