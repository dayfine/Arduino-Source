/*  Story Fake Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  A scripted game for engine tests, with no console and no model: each
 *  observe() returns the script's next look (the last one repeats), every
 *  input and log line is recorded, and a battle ends as scripted. A fault
 *  is a script: "black_without_warp" is a door that dims the screen and
 *  never warps.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryFakeBackend_H
#define PokemonAutomation_Pokemon_StoryFakeBackend_H

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>
#include "Pokemon/AutoStory/Types/Pokemon_GridPose.h"
#include "Pokemon_StoryBackend.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a FakeStoryBackend shows and does.
struct FakeStoryScript{
    //  observe()'s answers in order; the last repeats. None: an empty look.
    std::vector<Observation<GridPose>> looks = {};
    //  How each battle ends.
    Outcome battle = outcome_ok();
};

//  The backend of the engine's unit tests and of executor-level sim tests
//  that only need a script.
//  code-health: allow public_methods -- the eight backend calls it fakes plus the two recordings tests read; nothing to split
class FakeStoryBackend : public StoryBackend<GridPose>{
public:
    FakeStoryBackend(FakeStoryScript script, CancellableScope& scope);

    //  Each input, in order: "press A", "tap north", "hold east 400ms walk",
    //  "wait 100ms", "battle".
    const std::vector<std::string>& inputs() const{
        return m_inputs;
    }
    //  Each log line, in order.
    const std::vector<std::string>& lines() const{
        return m_lines;
    }

    virtual void press(NintendoSwitch::Button button, Milliseconds hold, Milliseconds release) override;
    virtual void tap(Direction direction) override;
    virtual void hold(Direction direction, Milliseconds duration, StoryPace pace) override;
    virtual void wait(Milliseconds duration) override;
    virtual Observation<GridPose> observe() override;
    virtual Outcome battle(const StoryActionArgs& args) override;
    virtual void log(std::string_view line) override;
    virtual CancellableScope& scope() override;

private:
    FakeStoryScript m_script;
    CancellableScope& m_scope;
    size_t m_next_look = 0;
    std::vector<std::string> m_inputs;
    std::vector<std::string> m_lines;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
