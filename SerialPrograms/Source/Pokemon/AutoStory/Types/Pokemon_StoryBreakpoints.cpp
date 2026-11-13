/*  Story Breakpoints
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryBreakpoints.h"

namespace PokemonAutomation{
namespace Pokemon{


std::string story_step_pause_label(std::string_view checkpoint_id, size_t step_index, std::string_view step_name){
    return std::format("{}/{} {}", checkpoint_id, step_index + 1, step_name);
}

std::string story_leg_pause_label(
    size_t index, size_t count, Direction direction, int tiles, const std::optional<TilePos>& destination
){
    std::string label = std::format("leg {}/{} {} {}", index + 1, count, direction_name(direction), tiles);
    if (destination){
        label += std::format(" -> ({}, {})", destination->x, destination->y);
    }
    return label;
}


StoryPausePoints::StoryPausePoints(StoryBreakpoints mode, StoryPauseHook hook)
    : m_mode(mode)
    , m_hook(std::move(hook))
{}

void StoryPausePoints::set_log(std::function<void(std::string_view line)> log){
    m_log = std::move(log);
}

void StoryPausePoints::before_step(const std::string& label){
    m_step_label = label;
    if (m_mode != StoryBreakpoints::OFF){
        pause(label);
    }
}

void StoryPausePoints::before_leg(const std::string& leg_label){
    if (m_mode == StoryBreakpoints::LEGS){
        pause(std::format("{} {}", m_step_label, leg_label));
    }
}

void StoryPausePoints::pause(const std::string& label){
    if (!m_hook){
        return;
    }
    if (m_log){
        m_log(std::format("[Break] {}: paused", label));
    }
    m_hook(label);
    if (m_log){
        m_log(std::format("[Break] {}: continued", label));
    }
}


namespace{

StoryPauseWait& installed_wait(){
    static StoryPauseWait wait;
    return wait;
}

} // namespace

void set_story_pause_wait(StoryPauseWait wait){
    installed_wait() = std::move(wait);
}

StoryPauseHook make_story_pause_hook(CancellableScope& scope){
    if (!installed_wait()){
        return {};
    }
    return [&scope](std::string_view label){
        installed_wait()(label, scope);
    };
}



} // namespace Pokemon
} // namespace PokemonAutomation
