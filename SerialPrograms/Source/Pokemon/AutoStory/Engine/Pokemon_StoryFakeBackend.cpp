/*  Story Fake Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include <utility>
#include "Pokemon_StoryFakeBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


namespace{


std::string button_name(NintendoSwitch::Button button){
    switch (button){
    case NintendoSwitch::BUTTON_A:
        return "A";
    case NintendoSwitch::BUTTON_B:
        return "B";
    case NintendoSwitch::BUTTON_X:
        return "X";
    case NintendoSwitch::BUTTON_Y:
        return "Y";
    case NintendoSwitch::BUTTON_PLUS:
        return "PLUS";
    default:
        return std::format("{}", (uint32_t)button);
    }
}


} // namespace


FakeStoryBackend::FakeStoryBackend(FakeStoryScript script, CancellableScope& scope)
    : m_script(std::move(script))
    , m_scope(scope)
{}

void FakeStoryBackend::press(NintendoSwitch::Button button, Milliseconds, Milliseconds){
    m_inputs.push_back("press " + button_name(button));
}

void FakeStoryBackend::tap(Direction direction){
    m_inputs.push_back("tap " + direction_name(direction));
}

void FakeStoryBackend::hold(Direction direction, Milliseconds duration, StoryPace pace){
    m_inputs.push_back(std::format(
        "hold {} {}ms {}", direction_name(direction), duration.count(), pace == StoryPace::RUN ? "run" : "walk"
    ));
}

void FakeStoryBackend::wait(Milliseconds duration){
    m_inputs.push_back(std::format("wait {}ms", duration.count()));
}

Observation<GridPose> FakeStoryBackend::observe(){
    if (m_script.looks.empty()){
        return {};
    }
    const size_t look = m_next_look < m_script.looks.size() ? m_next_look++ : m_script.looks.size() - 1;
    return m_script.looks[look];
}

Outcome FakeStoryBackend::battle(const StoryActionArgs&){
    m_inputs.push_back("battle");
    return m_script.battle;
}

void FakeStoryBackend::log(std::string_view line){
    m_lines.emplace_back(line);
}

CancellableScope& FakeStoryBackend::scope(){
    return m_scope;
}



} // namespace Pokemon
} // namespace PokemonAutomation
