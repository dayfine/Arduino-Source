/*  Story Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Input and observation, nothing else: what the story engine drives. One
 *  per run. A console backend wraps the console's controller and video and
 *  builds each observation with the game's detectors; a simulation backend
 *  wraps a model of the game. The engine and every action run the same on
 *  both.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryBackend_H
#define PokemonAutomation_Pokemon_StoryBackend_H

#include <optional>
#include <string_view>
#include "Common/Cpp/CancellableScope.h"
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenMachine/ScreenObservation.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/Navigation/Pokemon_Direction.h"
#include "Pokemon_StoryPoseConcept.h"
#include "Pokemon_ZonePose.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How fast a held direction moves the player.
enum class StoryPace{
    WALK,
    RUN,
};

//  What a backend plays on. An action declared console_only
//  (StoryActionSpec) runs only on CONSOLE: it reads or presses through
//  screens nothing else models (menus, a keyboard).
enum class StoryBackendKind{
    CONSOLE,
    SIMULATION,
    SCRIPT,     //  a scripted fake, for unit tests
};

//  "console", "simulation", "script".
std::string_view story_backend_kind_name(StoryBackendKind kind);

//  `direction` as degrees clockwise of straight on (0, 90, 180, -90): a
//  game with no tiles moves a direction's taps and holds as stick().
double direction_degrees(Direction direction);

//  One look at the game, with where the player is when a reader knows it
//  (StoryGame::readers fill `pose` and the readings).
template <StoryPose PoseType>
struct Observation : public ScreenObservation{
    std::optional<PoseType> pose = std::nullopt;
};

//  The engine's only way to the game.
//  code-health: allow public_methods -- the agreed backend boundary is these low-level calls; splitting them would make every action take two objects
template <StoryPose PoseType>
class StoryBackend{
public:
    virtual ~StoryBackend() = default;

    virtual StoryBackendKind kind() const = 0;

    virtual void press(NintendoSwitch::Button button, Milliseconds hold, Milliseconds release) = 0;
    //  One tile in `direction`, or a turn to face it.
    virtual void tap(Direction direction) = 0;
    virtual void hold(Direction direction, Milliseconds duration, StoryPace pace) = 0;
    //  The move stick held at `degrees` clockwise from north (straight on,
    //  for a camera-relative game) for `duration`, then let go for `release`
    //  (a turn's time to finish) like press(): the free-moving games' walk
    //  and turn. A tile game's hold() is this at a multiple of 90.
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace) = 0;
    virtual void wait(Milliseconds duration) = 0;
    //  The game now, once every input sent so far has been done.
    virtual Observation<PoseType> observe() = 0;
    //  Fights the battle on screen with the game's battle code, until it
    //  ends; a battle that cannot be won or does not end is a failed
    //  Outcome.
    virtual Outcome battle() = 0;
    virtual void log(std::string_view line) = 0;
    virtual CancellableScope& scope() = 0;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
