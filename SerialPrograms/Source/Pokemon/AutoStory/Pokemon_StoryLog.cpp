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


std::string event_log_line(
    std::string_view checkpoint, size_t number, std::string_view event, std::string_view status,
    std::string_view section
){
    if (section.empty()){
        return std::format("[Event] {}/{} {}: {}", checkpoint, number, event, status);
    }
    return std::format("[Event] {}/{} [{}] {}: {}", checkpoint, number, section, event, status);
}

std::string checkpoint_start_line(std::string_view checkpoint){
    return std::format("[Checkpoint] {}: start", checkpoint);
}

std::string checkpoint_start_line(std::string_view checkpoint, size_t events){
    return std::format("[Checkpoint] {}: start, {} events", checkpoint, events);
}

std::string checkpoint_attempt_line(std::string_view checkpoint, size_t attempt){
    return std::format("[Checkpoint] {}: attempt {}", checkpoint, attempt);
}

std::string checkpoint_attempt_line(std::string_view checkpoint, size_t attempt, size_t attempts){
    return std::format("[Checkpoint] {}: attempt {}/{}", checkpoint, attempt, attempts);
}

std::string checkpoint_pass_line(std::string_view checkpoint, size_t attempt, size_t in_place){
    return std::format(
        "[Checkpoint] {}: passed on attempt {} ({}){}",
        checkpoint, attempt, resets_text(attempt),
        in_place == 0 ? "" : std::format(", {} in place", in_place)
    );
}

std::string checkpoint_reset_line(std::string_view checkpoint, size_t next_attempt){
    return std::format("[Checkpoint] {}: failed, resetting for attempt {}", checkpoint, next_attempt);
}

std::string checkpoint_reset_line(std::string_view segment, std::string_view checkpoint, size_t next_attempt, size_t attempts){
    return std::format(
        "[Segment] {}: {} failed, reloading for attempt {}/{}",
        segment, checkpoint, next_attempt, attempts
    );
}


void record_story_counts(AutoStoryStats& stats, const StoryRunCounts& counts){
    stats.m_segment += counts.segments;
    stats.m_checkpoint += counts.checkpoints;
    stats.m_reset += counts.resets;
}



} // namespace Pokemon
} // namespace PokemonAutomation
