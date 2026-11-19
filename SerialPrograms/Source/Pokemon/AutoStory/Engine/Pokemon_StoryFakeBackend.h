/*  Story Fake Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A scripted backend for tests: it records every input and log line.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFakeBackend_H
#define PokemonAutomation_Pokemon_StoryFakeBackend_H

#include <cstddef>
#include <format>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a FakeStoryBackend shows and does.
template <StoryPose PoseType>
struct FakeStoryScript{
    //  observe()'s answers in order; the last repeats. None: an empty look.
    std::vector<Observation<PoseType>> looks = {};
    //  What it says it plays on: SCRIPT, or CONSOLE to run a console_only
    //  action's code in a test.
    StoryBackendKind kind = StoryBackendKind::SCRIPT;
    //  How each battle() ends.
    Outcome battle = outcome_ok();
};

//  Records every input and log line; observe() plays the script.
//  code-health: allow public_methods -- the backend calls it fakes plus the three recordings tests read; nothing to split
//  code-health: allow test_only_api -- a test double: the scripted backend the engine and game tests run on
template <StoryPose PoseType>
class FakeStoryBackend : public StoryBackend<PoseType>{
public:
    FakeStoryBackend(FakeStoryScript<PoseType> script, CancellableScope& scope)
        : m_script(std::move(script))
        , m_scope(scope)
    {}

    //  Each input, in order: "press A", "press +" (by button_to_string()),
    //  "stick 90deg 150ms walk", "stick 90deg 150ms walk, release 300ms"
    //  (a release only when not zero), "wait 100ms", "battle".
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

    virtual StoryBackendKind kind() const override{
        return m_script.kind;
    }
    virtual void press(NintendoSwitch::Button button, Milliseconds, Milliseconds) override{
        m_inputs.push_back(std::format("press {}", NintendoSwitch::button_to_string(button)));
    }
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace) override{
        std::string input = std::format("stick {}deg {}ms {}", degrees, duration.count(), pace == StoryPace::RUN ? "run" : "walk");
        if (release > Milliseconds::zero()){
            input += std::format(", release {}ms", release.count());
        }
        m_inputs.push_back(std::move(input));
    }
    virtual void wait(Milliseconds duration) override{
        m_inputs.push_back(std::format("wait {}ms", duration.count()));
    }
    virtual Observation<PoseType> observe() override{
        if (m_script.looks.empty()){
            return {};
        }
        const size_t look = m_next_look < m_script.looks.size() ? m_next_look++ : m_script.looks.size() - 1;
        return m_script.looks[look];
    }
    virtual Outcome battle(const StoryActionArgs&) override{
        m_inputs.push_back("battle");
        return m_script.battle;
    }
    virtual void log(std::string_view line) override{
        m_lines.emplace_back(line);
        m_overlay_lines.emplace_back(line);
    }
    virtual void log_detail(std::string_view line) override{
        m_lines.emplace_back(line);
    }
    virtual CancellableScope& scope() override{
        return m_scope;
    }

private:
    FakeStoryScript<PoseType> m_script;
    CancellableScope& m_scope;
    size_t m_next_look = 0;
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_lines;
    std::vector<std::string> m_overlay_lines;
};




} // namespace Pokemon
} // namespace PokemonAutomation
#endif
