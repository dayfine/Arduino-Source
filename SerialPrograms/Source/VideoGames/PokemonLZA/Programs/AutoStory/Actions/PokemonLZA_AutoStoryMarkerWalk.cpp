/*  PokemonLZA AutoStory - Marker walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <format>
#include <optional>
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryMarkerArrival.h"
#include "PokemonLZA_AutoStoryMarkerDistance.h"
#include "PokemonLZA_AutoStoryMarkerWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  A scene at the objective can take the marker away and start its talk up to 11 s later.
constexpr Milliseconds MARKER_WALK_LIMIT(90000);
constexpr Milliseconds MARKER_SCENE_WAIT(15000);
constexpr Milliseconds MARKER_POLL(250);

enum class MarkerStep{
    WALKING,
    ARRIVED,    //  MarkerArrival::arrived()
    LOST,       //  the marker did not show for 12 camera turns
};

class MarkerWalk{
public:
    MarkerWalk(StoryEnv& env, PlzaBackend& backend, MarkerProof proof)
        : m_env(env)
        , m_backend(backend)
        , m_arrival(proof)
    {}

    std::optional<int> position() const{
        return m_arrival.position();
    }

    MarkerArrival& arrival(){
        return m_arrival;
    }

    MarkerStep step(){
        const ScreenObservation look = m_backend.observe();
        std::optional<ObjectiveMarker> marker;
        if (look.frame && *look.frame){
            marker = read_objective_marker(m_env.env.console, *look.frame->frame);
        }
        if (at_objective(look, marker)){
            return MarkerStep::ARRIVED;
        }
        if (!marker){
            return look_behind();
        }
        m_unseen = 0;
        m_last_x = marker->x;
        //  Off screen, the game pins the marker to the edge with no digits.
        if (marker->x < 0.10 || marker->x > 0.90){
            pbf_move_right_joystick(m_env.context, {marker->x < 0.5 ? -0.5 : 0.5, 0}, /*hold=*/400ms, /*release=*/200ms);
            return MarkerStep::WALKING;
        }
        steer(marker->x);
        step_on();
        return MarkerStep::WALKING;
    }

private:
    //  A dialog not near the objective gets B (B cannot start a talk again).
    bool at_objective(const ScreenObservation& look, const std::optional<ObjectiveMarker>& marker){
        const bool prompt = look.events.has(ScreenEvent::INTERACT_PROMPT) || look.events.has(ScreenEvent::DIALOG_BOX);
        const std::optional<int> read = marker && marker->distance > 0 ? std::optional<int>(marker->distance) : std::nullopt;
        on_read(read, m_arrival.look(MarkerLook{
            .distance = read, .marker_shows = marker.has_value(), .prompt_or_dialog = prompt, .at = current_time(),
        }));
        if (m_arrival.arrived()){
            m_backend.log(std::format(
                "[PLZA] walk_to_marker: a prompt or dialog near the objective (last read taken {})", m_arrival.position().value_or(-1)
            ));
            return true;
        }
        if (prompt && !m_prompt_passed){
            m_backend.log(std::format(
                "[PLZA] walk_to_marker: a prompt or dialog, not near the objective (last read taken {}): walking on", m_arrival.position().value_or(-1)
            ));
        }
        m_prompt_passed = prompt;
        if (look.events.has(ScreenEvent::DIALOG_BOX)){
            pbf_press_button(m_env.context, BUTTON_B, /*hold=*/160ms, /*release=*/240ms);
        }
        return false;
    }


    MarkerStep look_behind(){
        if (++m_unseen > 12){
            return MarkerStep::LOST;
        }
        pbf_move_right_joystick(m_env.context, {m_last_x < 0.5 ? -0.5 : 0.5, 0}, /*hold=*/500ms, /*release=*/200ms);
        return MarkerStep::WALKING;
    }

    void on_read(std::optional<int> read, const MarkerRead& result){
        if (result.taken){
            on_distance(*read);
        }
        if (result.too_fast_after){
            m_backend.log(std::format(
                "[PLZA] walk_to_marker: read {} after {} in {:.1f} s: ignored",
                *read, m_arrival.position().value_or(-1), result.too_fast_after->count() / 1000.0
            ));
        }
    }

    //  4 reads with no new best = held up (e.g. by a fence): step sideways,
    //  right, then left twice as far, 4 at most.
    void on_distance(int distance){
        if (distance < m_best){
            m_backend.log(std::format("[PLZA] walk_to_marker: {} to go", distance));
            m_best = distance;
            m_still = 0;
            return;
        }
        if (++m_still < 4 || m_sidesteps >= 4){
            return;
        }
        m_still = 0;
        const bool right = m_sidesteps % 2 == 0;
        const Milliseconds time(1500 * (m_sidesteps + 1));
        m_backend.log(std::format("[PLZA] walk_to_marker: held up at {}: {} for {} ms", m_best, right ? "right" : "left", time.count()));
        pbf_move_left_joystick(m_env.context, {right ? 1.0 : -1.0, 0}, time, 200ms);
        m_sidesteps++;
    }

    void steer(double x){
        const double off = x - 0.5;
        if (off > 0.06 || off < -0.06){
            const Milliseconds turn((int)(std::min(1.0, std::abs(off) * 4) * 400) + 80);
            pbf_move_right_joystick(m_env.context, {off > 0 ? 0.4 : -0.4, 0}, turn, /*release=*/150ms);
        }
    }

    void step_on(){
        pbf_move_left_joystick(m_env.context, /*position=*/{0, 1}, /*hold=*/600ms, /*release=*/0ms);
    }

    StoryEnv& m_env;
    PlzaBackend& m_backend;
    MarkerArrival m_arrival;
    bool m_prompt_passed = false;   //  the last look showed a prompt or dialog not near the objective
    int m_best = 1 << 30;
    int m_still = 0;
    int m_sidesteps = 0;
    int m_unseen = 0;
    double m_last_x = 0.5;
};

//  After a walk that lost the marker: whether a prompt or dialog shows near it soon.
bool objective_reached(StoryEnv& env, PlzaBackend& backend, MarkerArrival& arrival){
    if (!arrival.near()){
        return false;
    }
    for (Milliseconds waited(0); ; waited += MARKER_POLL){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        const bool marker = look.frame && *look.frame && read_objective_marker(env.env.console, *look.frame->frame).has_value();
        arrival.look(MarkerLook{
            .distance = std::nullopt,
            .marker_shows = marker,
            .prompt_or_dialog = look.events.has(ScreenEvent::INTERACT_PROMPT) || look.events.has(ScreenEvent::DIALOG_BOX),
            .at = current_time(),
        });
        if (arrival.arrived()){
            return true;
        }
        if (waited >= MARKER_SCENE_WAIT){
            return false;
        }
        backend.wait(MARKER_POLL);
    }
}

bool run_marker_walk(Milliseconds limit, MarkerWalk& walk, PlzaBackend& backend){
    const WallClock deadline = current_time() + limit;
    while (current_time() < deadline){
        backend.scope().throw_if_cancelled();
        const MarkerStep step = walk.step();
        if (step == MarkerStep::LOST){
            backend.log("[PLZA] marker walk: the marker did not show for 12 camera turns");
            return false;
        }
        if (step == MarkerStep::ARRIVED){
            return true;
        }
    }
    backend.log(std::format("[PLZA] marker walk: out of time, last read taken {}", walk.position().value_or(-1)));
    return false;
}

} // namespace


std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend, std::optional<int> floor, int looks){
    MarkerDistance distance(floor);
    for (int tries = 0; tries < looks; tries++){
        backend.scope().throw_if_cancelled();
        const ScreenObservation look = backend.observe();
        if (look.frame && *look.frame){
            const std::optional<ObjectiveMarker> marker = read_objective_marker(env.env.console, *look.frame->frame);
            if (marker && marker->distance > 0 && distance.read(marker->distance)){
                backend.log(std::format("[PLZA] marker distance: {}", marker->distance));
                return marker->distance;
            }
        }
        backend.wait(MARKER_POLL);
    }
    backend.log(std::format("[PLZA] marker distance: no read taken in {} looks", looks));
    return std::nullopt;
}

std::optional<Pokemon::StoryFailure> walk_to_unread_marker(StoryEnv& env, PlzaBackend& backend){
    backend.log("[PLZA] walk_to_marker: to the objective's prompt or dialog, near it by no far read");
    MarkerWalk walk(env, backend, MarkerProof::NO_FAR_READ);
    if (run_marker_walk(MARKER_WALK_LIMIT, walk, backend)){
        return std::nullopt;
    }
    if (!objective_reached(env, backend, walk.arrival())){
        return Pokemon::StoryFailure{.code = "objective_not_reached", .detail = std::format(
            "no A prompt or dialog near the objective marker (last read taken {}, near: {})",
            walk.position().value_or(-1), walk.arrival().near() ? "yes" : "no"
        )};
    }
    return std::nullopt;
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
