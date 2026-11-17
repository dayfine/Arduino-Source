/*  PokemonLZA AutoStory - Walks between Wild Zone 1's road stops
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cstdint>
#include <format>
#include <optional>
#include <string>
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/Detectors/PokemonLZA_AutoStoryObjectiveMarkerReader.h"
#include "PokemonLZA_AutoStoryBracketWatch.h"
#include "PokemonLZA_AutoStoryZone1Road.h"
#include "PokemonLZA_AutoStoryRoadWalk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;


namespace{

//  Camera turns with no marker on screen before the walk gives up.
constexpr int MAX_UNSEEN = 12;
//  A held-up sidestep's hold, times the sidesteps so far.
constexpr Milliseconds SIDESTEP_UNIT(1500);

//  The walk, one look and one move at a time.
class RoadWalk{
public:
    RoadWalk(StoryEnv& env, PlzaBackend& backend, MarkerDirection direction, int stop_at, std::optional<int> floor)
        : m_env(env)
        , m_backend(backend)
        , m_direction(direction)
        , m_reads(direction, stop_at, floor)
    {}

    std::optional<int> position() const{
        return m_reads.position();
    }

    //  Returns whether the walk is over: arrived, or the marker lost.
    bool step(){
        const Pokemon::Observation<Pokemon::ZonePose> look = m_backend.observe();
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
            pbf_move_right_joystick(m_env.context, /*position=*/{marker->x < 0.5 ? -0.5 : 0.5, 0}, /*hold=*/400ms, /*release=*/200ms);
            return false;
        }
        if (on_read(marker->distance > 0 ? std::optional<int>(marker->distance) : std::nullopt)){
            return true;
        }
        const CameraTurn turn = camera_turn_toward(marker->x);
        if (turn.time.count() > 0){
            pbf_move_right_joystick(m_env.context, /*position=*/{turn.stick_x, 0}, turn.time, /*release=*/150ms);
        }
        const double forward = m_direction == MarkerDirection::BACK ? -1 : 1;
        pbf_move_left_joystick(m_env.context, /*position=*/{0, forward}, /*hold=*/600ms, /*release=*/0ms);
        return false;
    }

private:
    //  The marker is behind: the camera turned toward where it was last.
    bool look_behind(){
        if (++m_unseen > MAX_UNSEEN){
            m_backend.log("[PLZA] road walk: the marker did not show for 12 camera turns");
            return true;
        }
        pbf_move_right_joystick(m_env.context, /*position=*/{m_last_x < 0.5 ? -0.5 : 0.5, 0}, /*hold=*/500ms, /*release=*/200ms);
        return false;
    }

    //  A look's read: logged when taken and new, a sidestep when held up.
    //  Returns whether the walk arrived.
    bool on_read(std::optional<int> distance){
        const std::optional<int> before = m_reads.position();
        const RoadLook move = m_reads.next(distance);
        if (m_reads.position() != before){
            m_backend.log(std::format("[PLZA] road walk: {} to go", *m_reads.position()));
        }
        switch (move){
        case RoadLook::ARRIVED:
            m_backend.log(std::format("[PLZA] road walk: {} from the objective", *m_reads.position()));
            return true;
        case RoadLook::SIDESTEP_RIGHT:
        case RoadLook::SIDESTEP_LEFT:{
            const bool right = move == RoadLook::SIDESTEP_RIGHT;
            const Milliseconds hold = SIDESTEP_UNIT * static_cast<int64_t>(m_reads.sidesteps());
            m_backend.log(std::format("[PLZA] road walk: held up at {}: {} for {} ms", *m_reads.position(), right ? "right" : "left", hold.count()));
            pbf_move_left_joystick(m_env.context, /*position=*/{right ? 1.0 : -1.0, 0}, hold, /*release=*/200ms);
            return false;
        }
        case RoadLook::STEP:
            return false;
        }
        return false;
    }

    StoryEnv& m_env;
    PlzaBackend& m_backend;
    const MarkerDirection m_direction;
    RoadWalkReads m_reads;
    int m_unseen = 0;
    double m_last_x = 0.5;
};

} // namespace


std::optional<int> walk_to_road_stop(
    StoryEnv& env, PlzaBackend& backend,
    MarkerDirection direction, int stop_at, std::optional<int> floor, Milliseconds limit
){
    RoadWalk walk(env, backend, direction, stop_at, floor);
    const WallClock deadline = current_time() + limit;
    while (current_time() < deadline){
        backend.scope().throw_if_cancelled();
        if (walk.step()){
            return walk.position();
        }
    }
    backend.log(std::format("[PLZA] road walk: not at {} in time, last read taken {}", stop_at, walk.position().value_or(-1)));
    return walk.position();
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
