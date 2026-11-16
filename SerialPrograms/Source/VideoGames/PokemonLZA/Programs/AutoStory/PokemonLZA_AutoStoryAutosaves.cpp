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

//  The step after the step named `step` in `row`, if there is one.
std::optional<std::string_view> step_after(const Pokemon::StoryCheckpoint& row, std::string_view step){
    for (size_t index = 0; index + 1 < row.steps.size(); index++){
        if (row.steps[index].name == step){
            return row.steps[index + 1].name;
        }
    }
    return std::nullopt;
}

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
    virtual Outcome battle() override{
        return m_inner.battle();
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
    m_seen_in.reset();
    m_last_seen_in.reset();
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
    if (!m_seen_in){
        m_seen_in = std::string(step);
    }
    m_last_seen_in = std::string(step);
    if (new_sighting){
        const double seconds = (double)std::chrono::duration_cast<Milliseconds>(look.when - *m_origin).count() / 1000;
        backend.log(std::format("[PLZA] autosave badge at {:.1f}s in {}", seconds, step));
    }
}

std::optional<std::string> AutosaveSightings::finish(std::string_view last_step){
    std::optional<std::string> seen_in = std::move(m_last_seen_in);
    m_last_end_seen = seen_in == last_step;
    restart();
    return seen_in;
}


std::optional<std::string> autosave_step_error(
    const Pokemon::StoryCheckpoint& row, Pokemon::StoryRowSave save, std::optional<std::string_view> autosave_step
){
    if (!autosave_step){
        if (save == Pokemon::StoryRowSave::AUTOSAVE){
            return std::format("{}: \"save\": \"autosave\" needs \"autosave_step\", the step a run log showed the badge in", row.id);
        }
        return std::nullopt;
    }
    bool found = false;
    for (const Pokemon::StoryStep& step : row.steps){
        found = found || step.name == *autosave_step;
    }
    if (!found){
        return std::format("{}: autosave_step {} is not one of its steps", row.id, *autosave_step);
    }
    if (const std::optional<std::string_view> next = step_after(row, *autosave_step)){
        return std::format(
            "{}: the game autosaves in {}, before {}: the next checkpoint reloads that save, so end {} at {} and start the next one with {}",
            row.id, *autosave_step, *next, row.id, *autosave_step, *next
        );
    }
    return std::nullopt;
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
        const std::string_view last_step = checkpoint.steps.empty() ? std::string_view() : std::string_view(checkpoint.steps.back().name);
        const std::optional<std::string> seen_in = sightings.finish(last_step);
        if (sightings.last_end_seen()){
            backend.log(std::format("[Save] {}: the game autosaved (seen)", checkpoint.id));
            return Pokemon::outcome_ok();
        }
        if (seen_in){
            const std::string_view next = step_after(checkpoint, *seen_in).value_or(last_step);
            backend.log(std::format(
                "[Save] {}: the game autosaved in {}, before {}: not saved, a reload would skip {}",
                checkpoint.id, *seen_in, next, next
            ));
            return Pokemon::outcome_not_saved("autosaved before the last step");
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

Pokemon::StoryGame<ZonePose> reloading_only_to_start(const Pokemon::StoryGame<ZonePose>& game, const AutosaveSightings& sightings){
    Pokemon::StoryGame<ZonePose> guarded = game;
    if (!game.reload){
        return guarded;
    }
    guarded.reload = [reload = game.reload, &sightings](Pokemon::StoryBackend<ZonePose>& backend){
        if (const std::optional<std::string>& step = sightings.seen_in()){
            return Pokemon::outcome_failed("save_moved", std::format(
                "the game autosaved in {} during the failed run: a reload would load past the checkpoint's start", *step
            ));
        }
        return reload(backend);
    };
    return guarded;
}

void log_moved_save(PlzaBackend& backend, const AutosaveSightings& sightings, std::string_view checkpoint){
    if (const std::optional<std::string>& step = sightings.seen_in()){
        backend.log(std::format(
            "[Save] {}: the game autosaved in {} before the run failed: the latest save is past {}'s start",
            checkpoint, *step, checkpoint
        ));
    }
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
