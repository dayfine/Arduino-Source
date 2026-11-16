/*  Story Fake Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A scripted game for engine tests, with no console and no model: each
 *  observe() returns the script's next look (the last one repeats), every
 *  input and log line is recorded. A fault is a script: a walk whose fade
 *  never comes.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFakeBackend_H
#define PokemonAutomation_Pokemon_StoryFakeBackend_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon_StoryBackend.h"
#include "Pokemon_ZonePose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a FakeStoryBackend shows and does.
//  code-health: allow test_only_api -- a test double: tests build it by its fields, so no name use shows
template <StoryPose PoseType>
struct FakeStoryScript{
    //  observe()'s answers in order; the last repeats. None: an empty look.
    std::vector<Observation<PoseType>> looks = {};
    //  What it says it plays on: SCRIPT, or CONSOLE to run a console_only
    //  action's code in a test.
    StoryBackendKind kind = StoryBackendKind::SCRIPT;
};

//  The backend of the engine's unit tests and of a game's scripted tests,
//  for any pose type.
//  code-health: allow public_methods -- the backend calls it fakes plus the three recordings tests read; nothing to split
//  code-health: allow test_only_api -- a test double: the scripted backend the engine and game tests run on
template <StoryPose PoseType>
class FakeStoryBackend : public StoryBackend<PoseType>{
public:
    FakeStoryBackend(FakeStoryScript<PoseType> script, CancellableScope& scope);

    //  Each input, in order: "press A", "press +" (by button_to_string()),
    //  "tap north", "hold east 400ms walk", "stick 90deg 150ms walk",
    //  "stick 90deg 150ms walk, release 300ms" (a release only when not
    //  zero), "wait 100ms".
    const std::vector<std::string>& inputs() const{
        return m_inputs;
    }
    //  Each log line, in order, log_detail()'s too: the file log.
    const std::vector<std::string>& lines() const{
        return m_lines;
    }
    //  The lines log() wrote, in order: the stream's overlay.
    const std::vector<std::string>& overlay_lines() const{
        return m_overlay_lines;
    }

    virtual StoryBackendKind kind() const override;
    virtual void press(NintendoSwitch::Button button, Milliseconds hold, Milliseconds release) override;
    virtual void tap(Direction direction) override;
    virtual void hold(Direction direction, Milliseconds duration, StoryPace pace) override;
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;
    virtual Observation<PoseType> observe() override;
    virtual void log(std::string_view line) override;
    virtual void log_detail(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    FakeStoryScript<PoseType> m_script;
    CancellableScope& m_scope;
    size_t m_next_look = 0;
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_lines;
    std::vector<std::string> m_overlay_lines;
};

//  Instantiated for ZonePose (Pokemon_ZonePose.h), in
//  Pokemon_StoryFakeBackend.cpp.
extern template class FakeStoryBackend<ZonePose>;



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
