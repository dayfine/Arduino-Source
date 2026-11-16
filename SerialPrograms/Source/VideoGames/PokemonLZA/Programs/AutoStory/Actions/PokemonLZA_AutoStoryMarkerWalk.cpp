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

//  The marker's distance reads, with misreads dropped. A read far from the
//  last one is a misread: a 0.6 s step covers about 3. Below: 36 as "3" or
//  29 on console B; above: 110 as "1110" (2026-11-13), which then rejected
//  every true read after it. So three agreeing reads in a row far from the
//  last mean the last read was the wrong one, and they are taken.
class DistanceReads{
public:
    std::optional<int> last() const{
        return m_last;
    }

    //  Whether `distance` (a read above 0) is taken as the last read.
    bool accept(int distance){
        if (m_last && !near(distance, *m_last)){
            m_rejected = m_rejected_read && near(distance, *m_rejected_read) ? m_rejected + 1 : 1;
            m_rejected_read = distance;
            if (m_rejected < 3){
                return false;
            }
        }
        m_rejected_read.reset();
        m_rejected = 0;
        m_last = distance;
        return true;
    }

private:
    static bool near(int a, int b){
        return a >= b - 12 && a <= b + 12;
    }

    std::optional<int> m_last;
    std::optional<int> m_rejected_read;
    int m_rejected = 0;
};

//  How one step of the walk ended.
enum class MarkerStep{
    WALKING,
    ARRIVED,    //  an A prompt or a dialog shows
    LOST,       //  the marker did not show for 12 camera turns
};

//  The walk, one look and one move at a time.
class MarkerWalk{
public:
    MarkerWalk(StoryEnv& env, PlzaBackend& backend)
        : m_env(env)
        , m_backend(backend)
    {}

    MarkerStep step(){
        const Pokemon::Observation<Pokemon::ZonePose> look = m_backend.observe();
        if (look.events.has(ScreenEvent::INTERACT_PROMPT) || look.events.has(ScreenEvent::DIALOG_BOX)){
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
            pbf_move_right_joystick(m_env.context, {marker->x < 0.5 ? -0.5 : 0.5, 0}, 400ms, 200ms);
            return MarkerStep::WALKING;
        }
        if (marker->distance > 0 && m_reads.accept(marker->distance)){
            on_distance(marker->distance);
        }
        steer_and_step(marker->x);
        return MarkerStep::WALKING;
    }

private:
    //  The marker is behind: turn the camera toward where it was last.
    MarkerStep look_behind(){
        if (++m_unseen > 12){
            return MarkerStep::LOST;
        }
        pbf_move_right_joystick(m_env.context, {m_last_x < 0.5 ? -0.5 : 0.5, 0}, 500ms, 200ms);
        return MarkerStep::WALKING;
    }

    //  A taken read: logged when it changes; 4 reads with no new best is a
    //  walk held up (the Research Lab's front fence, console B 2026-11-10),
    //  which steps sideways: right, then left twice as far, 4 at most.
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

    //  Turns the camera toward the marker at `x` (0 to 1) and walks 0.6 s on.
    void steer_and_step(double x){
        const double off = x - 0.5;
        if (off > 0.06 || off < -0.06){
            const Milliseconds turn((int)(std::min(1.0, std::abs(off) * 4) * 400) + 80);
            pbf_move_right_joystick(m_env.context, {off > 0 ? 0.4 : -0.4, 0}, turn, 150ms);
        }
        pbf_move_left_joystick(m_env.context, {0, 1}, 600ms, 0ms);
    }

    StoryEnv& m_env;
    PlzaBackend& m_backend;
    DistanceReads m_reads;
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

} // namespace


Pokemon::Outcome walk_to_marker(StoryEnv& env, PlzaBackend& backend){
    MarkerWalk walk(env, backend);
    const WallClock deadline = current_time() + MARKER_WALK_LIMIT;
    while (current_time() < deadline){
        backend.scope().throw_if_cancelled();
        const MarkerStep step = walk.step();
        if (step == MarkerStep::LOST){
            backend.log("[PLZA] walk_to_marker: the marker did not show for 12 camera turns");
            break;
        }
        if (step == MarkerStep::ARRIVED){
            break;
        }
    }
    if (!objective_reached(backend)){
        return Pokemon::outcome_failed("objective_not_reached", "no A prompt or dialog at the objective marker");
    }
    return Pokemon::outcome_ok();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
