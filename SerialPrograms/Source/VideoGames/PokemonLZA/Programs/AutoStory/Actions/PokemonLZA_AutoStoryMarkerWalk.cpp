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
#include "PokemonLZA_AutoStoryMarkerDistance.h"
#include "PokemonLZA_AutoStoryMarkerWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  How long the walk may take, and how long it then waits for the
//  objective's prompt or dialog: a scene at the objective (03.09's lab
//  gate) takes the marker away first and starts its talk 9 to 11 s later
//  (console B, 2026-11-14).
constexpr Milliseconds MARKER_WALK_LIMIT(90000);
constexpr Milliseconds MARKER_SCENE_WAIT(15000);
constexpr Milliseconds MARKER_POLL(250);

//  How one step of the walk ended.
enum class MarkerStep{
    WALKING,
    ARRIVED,    //  at the goal's distance (MarkerDistance::reached()), or (no distance) an A prompt or a dialog shows
    LOST,       //  the marker did not show for 12 camera turns
};

//  Where a walk ends.
struct MarkerGoal{
    //  The distance that ends it; none: an A prompt or a dialog does.
    std::optional<int> stop_at;
    //  Back away from the marker, until it is `stop_at` or more; or on
    //  toward it, until `stop_at` or less.
    MarkerDirection direction = MarkerDirection::TOWARD;
};

//  The walk, one look and one move at a time.
class MarkerWalk{
public:
    MarkerWalk(StoryEnv& env, PlzaBackend& backend, MarkerGoal goal)
        : m_env(env)
        , m_backend(backend)
        , m_goal(goal)
        , m_distance(goal.direction)
    {}

    //  Where the walk is (MarkerDistance::position()), if a distance read.
    std::optional<int> position() const{
        return m_distance.position();
    }

    MarkerStep step(){
        const Pokemon::Observation<Pokemon::ZonePose> look = m_backend.observe();
        if (!m_goal.stop_at && (look.events.has(ScreenEvent::INTERACT_PROMPT) || look.events.has(ScreenEvent::DIALOG_BOX))){
            return MarkerStep::ARRIVED;
        }
        std::optional<ObjectiveMarker> marker;
        if (look.frame && *look.frame){
            marker = read_objective_marker(m_env.env.console, *look.frame->frame);
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
        return on_marker(*marker);
    }

private:
    //  A marker on screen: its distance read is taken (or dropped as a
    //  misread); at the goal's distance, by the last read and the steps
    //  since, the walk ends; else the camera is turned to it and a step on.
    //  With the digits not read for a stretch, the steps count on from the
    //  last read: walking on blind took 03.08's walk to 100 past it and out
    //  of Wild Zone 1 (console B, 2026-11-16, 109 to 37).
    MarkerStep on_marker(const ObjectiveMarker& marker){
        const bool taken = marker.distance > 0 && m_distance.read(marker.distance);
        if (reached()){
            log_arrival();
            return MarkerStep::ARRIVED;
        }
        if (taken){
            on_distance(marker.distance);
        }
        steer_and_step(marker.x);
        return MarkerStep::WALKING;
    }

    void log_arrival() const{
        const int at = *m_distance.position();
        if (m_distance.steps_unread() == 0){
            m_backend.log(std::format("[PLZA] marker walk: {} from the objective", at));
            return;
        }
        m_backend.log(std::format(
            "[PLZA] marker walk: about {} from the objective ({} steps since the last read, {})",
            at, m_distance.steps_unread(), *m_distance.last_read()
        ));
    }

    //  The marker is behind: turn the camera toward where it was last.
    MarkerStep look_behind(){
        if (++m_unseen > 12){
            return MarkerStep::LOST;
        }
        pbf_move_right_joystick(m_env.context, {m_last_x < 0.5 ? -0.5 : 0.5, 0}, /*hold=*/500ms, /*release=*/200ms);
        return MarkerStep::WALKING;
    }

    bool reached() const{
        return m_goal.stop_at && m_distance.reached(*m_goal.stop_at);
    }

    //  A taken read toward the marker: logged when it changes; 4 reads with
    //  no new best is a walk held up (the Research Lab's front fence,
    //  console B 2026-11-10), which steps sideways: right, then left twice
    //  as far, 4 at most.
    void on_distance(int distance){
        if (m_goal.direction == MarkerDirection::BACK){
            return;
        }
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

    //  Turns the camera toward the marker at `x` (0 to 1) and walks 0.6 s
    //  on, or back toward the camera. Walking back turns the camera, so it
    //  is turned back onto the marker before each step: unsteered, the walk
    //  from 60 to 100 bent off Wild Zone 1's road (console B, 2026-11-14).
    void steer_and_step(double x){
        const double off = x - 0.5;
        if (off > 0.06 || off < -0.06){
            const Milliseconds turn((int)(std::min(1.0, std::abs(off) * 4) * 400) + 80);
            pbf_move_right_joystick(m_env.context, {off > 0 ? 0.4 : -0.4, 0}, turn, /*release=*/150ms);
        }
        const double forward = m_goal.direction == MarkerDirection::BACK ? -1 : 1;
        pbf_move_left_joystick(m_env.context, /*position=*/{0, forward}, /*hold=*/600ms, /*release=*/0ms);
        m_distance.stepped();
    }

    StoryEnv& m_env;
    PlzaBackend& m_backend;
    const MarkerGoal m_goal;
    MarkerDistance m_distance;
    int m_best = 1 << 30;
    int m_still = 0;
    int m_sidesteps = 0;
    int m_unseen = 0;
    double m_last_x = 0.5;
};

//  Whether an A prompt or a dialog shows within MARKER_SCENE_WAIT.
bool objective_reached(PlzaBackend& backend){
    for (Milliseconds waited(0); ; waited += MARKER_POLL){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (look.events.has(ScreenEvent::INTERACT_PROMPT) || look.events.has(ScreenEvent::DIALOG_BOX)){
            return true;
        }
        if (waited >= MARKER_SCENE_WAIT){
            return false;
        }
        backend.wait(MARKER_POLL);
    }
}

//  `walk` until it arrives, loses the marker or `limit` passes. Returns
//  whether it arrived.
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
    backend.log(std::format("[PLZA] marker walk: out of time, at {}", walk.position().value_or(-1)));
    return false;
}

} // namespace


std::optional<int> walk_toward_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, Milliseconds limit){
    MarkerWalk walk(env, backend, MarkerGoal{.stop_at = stop_at, .direction = MarkerDirection::TOWARD});
    run_marker_walk(limit, walk, backend);
    return walk.position();
}

std::optional<int> walk_back_from_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, Milliseconds limit){
    MarkerWalk walk(env, backend, MarkerGoal{.stop_at = stop_at, .direction = MarkerDirection::BACK});
    run_marker_walk(limit, walk, backend);
    return walk.position();
}

std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend){
    MarkerDistance distance(MarkerDirection::TOWARD);
    for (int looks = 0; looks < 8; looks++){
        backend.scope().throw_if_cancelled();
        const Pokemon::Observation<Pokemon::ZonePose> look = backend.observe();
        if (look.frame && *look.frame){
            const std::optional<ObjectiveMarker> marker = read_objective_marker(env.env.console, *look.frame->frame);
            if (marker && marker->distance > 0 && distance.read(marker->distance)){
                backend.log(std::format("[PLZA] marker distance: {}", marker->distance));
                return marker->distance;
            }
        }
        backend.wait(MARKER_POLL);
    }
    backend.log("[PLZA] marker distance: no read taken in 8 looks");
    return std::nullopt;
}

Pokemon::Outcome walk_to_marker(StoryEnv& env, PlzaBackend& backend){
    MarkerWalk walk(env, backend, MarkerGoal{});
    run_marker_walk(MARKER_WALK_LIMIT, walk, backend);
    if (!objective_reached(backend)){
        return Pokemon::outcome_failed("objective_not_reached", "no A prompt or dialog at the objective marker");
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
