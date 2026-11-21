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


//  "walk" or "run".
std::string_view pace_name(StoryPace pace){
    return pace == StoryPace::RUN ? "run" : "walk";
}


} // namespace


template <StoryPose PoseType>
FakeStoryBackend<PoseType>::FakeStoryBackend(FakeStoryScript<PoseType> script, CancellableScope& scope)
    : m_script(std::move(script))
    , m_scope(scope)
{}

template <StoryPose PoseType>
StoryBackendKind FakeStoryBackend<PoseType>::kind() const{
    return m_script.kind;
}

template <StoryPose PoseType>
void FakeStoryBackend<PoseType>::press(NintendoSwitch::Button button, Milliseconds, Milliseconds){
    m_inputs.push_back(std::format("press {}", NintendoSwitch::button_to_string(button)));
}

template <StoryPose PoseType>
void FakeStoryBackend<PoseType>::tap(Direction direction){
    m_inputs.push_back(std::format("tap {}", direction_name(direction)));
}

template <StoryPose PoseType>
void FakeStoryBackend<PoseType>::hold(Direction direction, Milliseconds duration, StoryPace pace){
    m_inputs.push_back(std::format("hold {} {}ms {}", direction_name(direction), duration.count(), pace_name(pace)));
}

template <StoryPose PoseType>
void FakeStoryBackend<PoseType>::stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace){
    std::string input = std::format("stick {}deg {}ms {}", degrees, duration.count(), pace_name(pace));
    if (release > Milliseconds::zero()){
        input += std::format(", release {}ms", release.count());
    }
    m_inputs.push_back(std::move(input));
}

template <StoryPose PoseType>
void FakeStoryBackend<PoseType>::wait(Milliseconds duration){
    m_inputs.push_back(std::format("wait {}ms", duration.count()));
}

template <StoryPose PoseType>
Observation<PoseType> FakeStoryBackend<PoseType>::observe(){
    if (m_script.looks.empty()){
        return {};
    }
    const size_t look = m_next_look < m_script.looks.size() ? m_next_look++ : m_script.looks.size() - 1;
    return m_script.looks[look];
}

template <StoryPose PoseType>
Outcome FakeStoryBackend<PoseType>::battle(const StoryActionArgs&){
    m_inputs.push_back("battle");
    return m_script.battle;
}

template <StoryPose PoseType>
void FakeStoryBackend<PoseType>::log(std::string_view line){
    m_lines.emplace_back(line);
}

template <StoryPose PoseType>
CancellableScope& FakeStoryBackend<PoseType>::scope(){
    return m_scope;
}


template class FakeStoryBackend<GridPose>;
template class FakeStoryBackend<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
