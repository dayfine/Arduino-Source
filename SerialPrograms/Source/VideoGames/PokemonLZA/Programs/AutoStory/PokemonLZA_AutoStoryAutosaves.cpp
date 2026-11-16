/*  PokemonLZA AutoStory - Autosaves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <chrono>
#include <format>
#include <string>
#include <utility>
#include "PokemonLZA_AutoStoryReadings.h"
#include "PokemonLZA_AutoStoryAutosaves.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Outcome;
using Pokemon::StoryActionCall;
using Pokemon::ZonePose;


namespace{

//  The autosave badge shows 1-3 s: a badge this soon after the last one is
//  the same autosave.
constexpr Milliseconds AUTOSAVE_DEBOUNCE = 5000ms;

//  `inner` with every look noted in `sightings` as taken in step `step`.
//  code-health: allow public_methods -- forwards every call of the engine's backend to the wrapped one unchanged
class SightingBackend : public PlzaBackend{
public:
    SightingBackend(PlzaBackend& inner, AutosaveSightings& sightings, std::string_view step)
        : m_inner(inner)
        , m_sightings(sightings)
        , m_step(step)
    {}

    virtual Pokemon::StoryBackendKind kind() const override{
        return m_inner.kind();
    }
    virtual void press(Button button, Milliseconds hold, Milliseconds release) override{
        m_inner.press(button, hold, release);
    }
    virtual void tap(Pokemon::Direction direction) override{
        m_inner.tap(direction);
    }
    virtual void hold(Pokemon::Direction direction, Milliseconds duration, Pokemon::StoryPace pace) override{
        m_inner.hold(direction, duration, pace);
    }
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace pace) override{
        m_inner.stick(degrees, duration, release, pace);
    }
    virtual void wait(Milliseconds duration) override{
        m_inner.wait(duration);
    }
    virtual Pokemon::Observation<ZonePose> observe() override{
        Pokemon::Observation<ZonePose> look = m_inner.observe();
        m_sightings.note(m_inner, look, m_step);
        return look;
    }
    virtual void log(std::string_view line) override{
        m_inner.log(line);
    }
    virtual void log_detail(std::string_view line) override{
        m_inner.log_detail(line);
    }
    virtual CancellableScope& scope() override{
        return m_inner.scope();
    }

private:
    PlzaBackend& m_inner;
    AutosaveSightings& m_sightings;
    std::string_view m_step;
};

} // namespace


void AutosaveSightings::restart(){
    m_origin.reset();
    m_last_badge.reset();
    m_seen = false;
}

void AutosaveSightings::note(PlzaBackend& backend, const Pokemon::Observation<ZonePose>& look, std::string_view step){
    if (!m_origin){
        m_origin = look.when;
    }
    if (!look.readings.contains(AUTOSAVE_READING)){
        return;
    }
    const bool new_sighting = !m_last_badge || look.when - *m_last_badge >= AUTOSAVE_DEBOUNCE;
    m_last_badge = look.when;
    m_seen = true;
    if (new_sighting){
        const double seconds = (double)std::chrono::duration_cast<Milliseconds>(look.when - *m_origin).count() / 1000;
        backend.log(std::format("[PLZA] autosave badge at {:.1f}s in {}", seconds, step));
    }
}

bool AutosaveSightings::finish(){
    m_last_end_seen = m_seen;
    restart();
    return m_last_end_seen;
}


Pokemon::StoryGame<ZonePose> with_autosave_sightings(
    Pokemon::StoryGame<ZonePose> game, const Pokemon::StoryRows& rows, AutosaveSightings& sightings
){
    for (Pokemon::StoryAction<ZonePose>& action : game.actions){
        action.handler = [&sightings, handler = std::move(action.handler)](
            Pokemon::StoryBackend<ZonePose>& backend, const StoryActionCall& call, Pokemon::RunState& state
        ){
            SightingBackend noted(backend, sightings, call.step.name);
            return handler(noted, call, state);
        };
    }
    game.save = [rows, &sightings](Pokemon::StoryBackend<ZonePose>& backend, const Pokemon::StoryCheckpoint& checkpoint){
        if (sightings.finish()){
            backend.log(std::format("[Save] {}: the game autosaved (seen)", checkpoint.id));
            return Pokemon::outcome_ok();
        }
        if (rows.unverified_autosaves.contains(checkpoint.id)){
            return Pokemon::outcome_not_saved("save unverified");
        }
        if (rows.autosaves.contains(checkpoint.id)){
            return Pokemon::outcome_not_saved("the plan's autosave was not seen");
        }
        return Pokemon::outcome_not_saved("no autosave seen");
    };
    return game;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
