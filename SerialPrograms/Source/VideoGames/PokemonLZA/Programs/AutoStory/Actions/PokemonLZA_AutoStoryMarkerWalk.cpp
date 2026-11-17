/*  PokemonLZA AutoStory - Marker walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <algorithm>
#include <cmath>
#include <cstdlib>
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

//  A camera nudge (MarkerMove::NUDGE_CAMERA_*) turns the camera until the
//  marker sits this far (of the screen's width) off the middle, by the
//  marker's own x, so it stays well on screen (0.38 or 0.62; the game pins
//  it to the edge below 0.10 and above 0.90). Not measured on console in
//  degrees; about 8 if the view spans 70.
constexpr double NUDGE_OFFSET = 0.12;
//  One lane of a sidestep (MarkerMove::SIDESTEP_*), the stick full left
//  or right. Not measured on console: shorter than a 600 ms step on.
constexpr Milliseconds SIDESTEP_HOLD(500);

//  How one step of the walk ended.
enum class MarkerStep{
    WALKING,
    ARRIVED,    //  at the goal's distance (MarkerMove::ARRIVED), or (no distance) an A prompt or a dialog shows
    LOST,       //  the marker did not show for 12 camera turns
    BLOCKED,    //  no distance read taken through every new view (MarkerMove::BLOCKED)
};

//  Where a walk ends.
struct MarkerGoal{
    //  The distance that ends it; none: an A prompt or a dialog does.
    std::optional<int> stop_at;
    //  Back away from the marker, until it is `stop_at` or more; or on
    //  toward it, until `stop_at` or less.
    MarkerDirection direction = MarkerDirection::TOWARD;
    //  Reads below it are not taken; a walk toward ends at it.
    std::optional<int> floor;
};

//  The walk, one look and one move at a time.
class MarkerWalk{
public:
    MarkerWalk(StoryEnv& env, PlzaBackend& backend, MarkerGoal goal)
        : m_env(env)
        , m_backend(backend)
        , m_goal(goal)
        , m_distance(goal.direction, goal.floor)
    {}

    //  The last distance read taken, if any.
    std::optional<int> position() const{
        return m_distance.position();
    }

    //  Back onto the marker's line, so the next walk starts on it.
    void to_line(){
        to_lane(0);
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
    //  A marker on screen. With no distance to stop at (the walk ends at a
    //  prompt), a read taken counts toward a held-up walk, and the camera
    //  is turned to it and a step on. Else its read decides
    //  (MarkerDistance): a step on only on a read taken; with none, the
    //  walk stands and reads again, trying new views (new_view()) until
    //  one reads or it is blocked.
    //  Stepping on blind took 03.08's walk to 100 from 109 out through the
    //  far gate of Wild Zone 1 (console B, 2026-11-16, 37 by a market
    //  stall).
    MarkerStep on_marker(const ObjectiveMarker& marker){
        if (!m_goal.stop_at){
            if (marker.distance > 0 && m_distance.read(marker.distance)){
                on_distance(marker.distance);
            }
            steer(marker.x, /*aim=*/0.5);
            step_on();
            return MarkerStep::WALKING;
        }
        const std::optional<int> read = marker.distance > 0 ? std::optional<int>(marker.distance) : std::nullopt;
        const MarkerMove move = m_distance.next(read, *m_goal.stop_at);
        switch (move){
        case MarkerMove::ARRIVED:
            m_backend.log(std::format("[PLZA] marker walk: {} from the objective", *m_distance.position()));
            to_line();
            return MarkerStep::ARRIVED;
        case MarkerMove::BLOCKED:
            m_backend.log(std::format(
                "[PLZA] marker walk: blocked, no distance taken after 2 camera nudges and 4 sidesteps (read {}, at {})",
                read.value_or(-1), m_distance.position().value_or(-1)
            ));
            to_line();
            return MarkerStep::BLOCKED;
        case MarkerMove::STEP:
            break;
        default:
            new_view(move, marker, read);
            return MarkerStep::WALKING;
        }
        m_unread = 0;
        m_aim = 0.5;
        if (m_distance.position() != m_logged){
            m_logged = m_distance.position();
            m_backend.log(std::format("[PLZA] marker walk: {} to go", *m_logged));
        }
        steer(marker.x, m_aim);
        to_lane(m_distance.lane());
        step_on();
        return MarkerStep::WALKING;
    }

    //  No read taken on this look: no step on, the view `move` asks for
    //  (MarkerMove), the camera turned to the walk's aim, a look again
    //  after MARKER_POLL. Logged at the first look of a stretch, every
    //  8th, and at each new view.
    void new_view(MarkerMove move, const ObjectiveMarker& marker, std::optional<int> read){
        if (m_unread++ % 8 == 0){
            m_backend.log(std::format(
                "[PLZA] marker walk: no distance taken (read {}, at {}): standing, re-reading",
                read.value_or(-1), m_distance.position().value_or(-1)
            ));
        }
        switch (move){
        case MarkerMove::RECENTER:
            m_aim = 0.5;
            break;
        case MarkerMove::NUDGE_CAMERA_LEFT:
        case MarkerMove::NUDGE_CAMERA_RIGHT:
            //  The camera turned left moves the marker right on screen.
            m_aim = move == MarkerMove::NUDGE_CAMERA_LEFT ? 0.5 + NUDGE_OFFSET : 0.5 - NUDGE_OFFSET;
            m_backend.log(std::format(
                "[PLZA] marker walk: nudging the camera {}, the marker to {:.2f}",
                move == MarkerMove::NUDGE_CAMERA_LEFT ? "left" : "right", m_aim
            ));
            break;
        case MarkerMove::SIDESTEP_LEFT:
        case MarkerMove::SIDESTEP_RIGHT:
            m_aim = 0.5;
            steer(marker.x, m_aim);
            to_lane(m_distance.lane());
            m_backend.wait(MARKER_POLL);
            return;
        default:
            break;
        }
        steer(marker.x, m_aim);
        m_backend.wait(MARKER_POLL);
    }

    //  Steps across the marker's line (the camera on the marker) to
    //  `lane` (MarkerDistance::lane()), from the lane the walk stands in.
    void to_lane(int lane){
        if (lane == m_lane){
            return;
        }
        const bool left = lane < m_lane;
        const int steps = std::abs(lane - m_lane);
        m_backend.log(std::format(
            "[PLZA] marker walk: sidestepping {} {} step(s), to lane {} (at {})",
            left ? "left" : "right", steps, lane, m_distance.position().value_or(-1)
        ));
        pbf_move_left_joystick(m_env.context, /*position=*/{left ? -1.0 : 1.0, 0}, SIDESTEP_HOLD * steps, /*release=*/200ms);
        m_lane = lane;
    }

    //  The marker is behind: turn the camera toward where it was last.
    MarkerStep look_behind(){
        if (++m_unseen > 12){
            return MarkerStep::LOST;
        }
        pbf_move_right_joystick(m_env.context, {m_last_x < 0.5 ? -0.5 : 0.5, 0}, /*hold=*/500ms, /*release=*/200ms);
        return MarkerStep::WALKING;
    }

    //  A taken read on a walk to the objective's prompt (no stop_at; a
    //  walk to a stop is held up by MarkerDistance): logged when it
    //  changes; 4 reads with no new best is a walk held up (the Research
    //  Lab's front fence, console B 2026-11-10), which steps sideways:
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

    //  Turns the camera until the marker at `x` (0 to 1) sits at `aim`
    //  (the middle, or a nudge off it). Walking back turns the camera, so
    //  it is turned back onto the marker before each step: unsteered, the
    //  walk from 60 to 100 bent off Wild Zone 1's road (console B,
    //  2026-11-14).
    void steer(double x, double aim){
        const double off = x - aim;
        if (off > 0.06 || off < -0.06){
            const Milliseconds turn((int)(std::min(1.0, std::abs(off) * 4) * 400) + 80);
            pbf_move_right_joystick(m_env.context, {off > 0 ? 0.4 : -0.4, 0}, turn, /*release=*/150ms);
        }
    }

    //  0.6 s on, or back toward the camera.
    void step_on(){
        const double forward = m_goal.direction == MarkerDirection::BACK ? -1 : 1;
        pbf_move_left_joystick(m_env.context, /*position=*/{0, forward}, /*hold=*/600ms, /*release=*/0ms);
    }

    StoryEnv& m_env;
    PlzaBackend& m_backend;
    const MarkerGoal m_goal;
    MarkerDistance m_distance;
    int m_best = 1 << 30;
    int m_still = 0;
    int m_sidesteps = 0;
    int m_unseen = 0;
    int m_unread = 0;
    int m_lane = 0;         //  the lane the walk stands in (MarkerDistance::lane())
    std::optional<int> m_logged;    //  the last read taken that was logged
    double m_aim = 0.5;     //  where on screen the camera keeps the marker
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
        if (step == MarkerStep::BLOCKED){
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


std::optional<int> walk_toward_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, int floor, Milliseconds limit){
    MarkerWalk walk(env, backend, MarkerGoal{.stop_at = stop_at, .direction = MarkerDirection::TOWARD, .floor = floor});
    run_marker_walk(limit, walk, backend);
    walk.to_line();
    return walk.position();
}

std::optional<int> walk_back_from_marker(StoryEnv& env, PlzaBackend& backend, int stop_at, Milliseconds limit){
    MarkerWalk walk(env, backend, MarkerGoal{.stop_at = stop_at, .direction = MarkerDirection::BACK, .floor = std::nullopt});
    run_marker_walk(limit, walk, backend);
    walk.to_line();
    return walk.position();
}

std::optional<int> read_marker_distance(StoryEnv& env, PlzaBackend& backend, std::optional<int> floor, int looks){
    MarkerDistance distance(MarkerDirection::TOWARD, floor);
    for (int look = 0; look < looks; look++){
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
    backend.log(std::format("[PLZA] marker distance: no read taken in {} looks", looks));
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
