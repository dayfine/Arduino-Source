/*  PokemonLZA AutoStory - Story backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <cmath>
#include <numbers>
#include <string>
#include "Common/Cpp/Color.h"
#include "CommonFramework/ImageTools/ImageDiff.h"
#include "NintendoSwitch/Commands/NintendoSwitch_Commands_PushButtons.h"
#include "VideoGames/PokemonLZA/Programs/PokemonLZA_BasicNavigation.h"
#include "PokemonLZA_AutoStoryActions.h"
#include "PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{

using namespace std::chrono_literals;
using Pokemon::Direction;


namespace{

//  Below this pixel RMSD two looks are the same screen.
constexpr double STILL_RMSD = 2.0;

//  `direction` as degrees clockwise of straight on.
double direction_degrees(Direction direction){
    switch (direction){
    case Direction::NORTH:
        return 0;
    case Direction::EAST:
        return 90;
    case Direction::SOUTH:
        return 180;
    case Direction::WEST:
        return -90;
    }
    return 0;
}

bool same_screen(const VideoSnapshot& a, const VideoSnapshot& b){
    if (!a || !b || a.frame->width() != b.frame->width() || a.frame->height() != b.frame->height()){
        return false;
    }
    return ImageMatch::pixel_RMSD(*a.frame, *b.frame) < STILL_RMSD;
}

} // namespace


PlzaConsoleBackend::PlzaConsoleBackend(StoryEnv& env)
    : m_env(env)
{}

void PlzaConsoleBackend::press(Button button, Milliseconds hold, Milliseconds release){
    pbf_press_button(m_env.context, button, hold, release);
}

void PlzaConsoleBackend::tap(Direction direction){
    story_turn(*this, direction_degrees(direction));
}

void PlzaConsoleBackend::hold(Direction direction, Milliseconds duration, Pokemon::StoryPace pace){
    stick(direction_degrees(direction), duration, 0ms, pace);
}

void PlzaConsoleBackend::stick(double degrees, Milliseconds duration, Milliseconds release, Pokemon::StoryPace){
    const double radians = degrees * std::numbers::pi / 180.0;
    pbf_move_left_joystick(m_env.context, {std::sin(radians), std::cos(radians)}, duration, release);
}

void PlzaConsoleBackend::wait(Milliseconds duration){
    m_env.context.wait_for(duration);
}

Pokemon::Observation<Pokemon::ZonePose> PlzaConsoleBackend::observe(){
    m_env.context.wait_for_all_requests();
    VideoSnapshot frame = m_env.env.console.video().snapshot();
    Pokemon::Observation<Pokemon::ZonePose> look;
    if (frame){
        static_cast<ScreenObservation&>(look) = plza_observation(m_observer.read(*frame.frame));
        if (same_screen(m_previous, frame)){
            look.events.add(PokemonAutomation::ScreenEvent::STILL);
        }
    }
    look.when = current_time();
    m_previous = frame;
    look.frame = std::move(frame);
    return look;
}

Pokemon::Outcome PlzaConsoleBackend::battle(const Pokemon::StoryActionArgs&){
    try{
        fight_battle(m_env.env, m_env.env.console, m_env.context, m_env.battle_ai);
    }catch (StoryActionNotImplemented&){
        throw;
    }catch (OperationFailedException& e){
        return Pokemon::outcome_failed("battle_failed", e.message());
    }
    return Pokemon::outcome_ok();
}

void PlzaConsoleBackend::log(std::string_view line){
    m_env.env.console.log(std::string(line), COLOR_BLUE);
}

CancellableScope& PlzaConsoleBackend::scope(){
    return m_env.context;
}

void story_turn(PlzaBackend& backend, double degrees){
    //  150 ms is the tap the story was recorded with (console B, 2026-11-08);
    //  the 300 ms release after it lets the turn finish.
    backend.press(BUTTON_L, 100ms, 400ms);
    backend.stick(degrees, 150ms, 300ms, Pokemon::StoryPace::WALK);
}

PlzaReader heading_reader(StoryEnv& env){
    return [&env](PlzaBackend&, Pokemon::Observation<Pokemon::ZonePose>& observation){
        const double heading = get_facing_direction(env.env.console, env.context);
        observation.readings[std::string(Pokemon::STORY_HEADING)] = (int)std::lround(heading) % 360;
    };
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
