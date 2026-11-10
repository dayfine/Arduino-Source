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
#include "Pokemon/AutoStory/Types/Pokemon_StoryActionArg.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
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
//  screens nothing else models (menus, a keyboard, a battle AI).
enum class StoryBackendKind{
    CONSOLE,
    SIMULATION,
    SCRIPT,     //  a scripted fake, for unit tests
};

//  "console", "simulation", "script".
std::string_view story_backend_kind_name(StoryBackendKind kind);

//  The reading (ScreenObservation::readings) of the way the player faces in
//  a game without tiles: degrees clockwise from north (straight on, for a
//  camera-relative game), in [0, 360). A backend that can read it cheaply
//  sets it in each observe(); otherwise a game's reader of this name does.
inline constexpr std::string_view STORY_HEADING = "heading";

//  `observation`'s STORY_HEADING reading; none when nothing read it.
std::optional<int> story_heading(const ScreenObservation& observation);

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
    //  for a camera-relative game) for `duration`: the free-moving games'
    //  walk and turn. A tile game's hold() is this at a multiple of 90.
    virtual void stick(double degrees, Milliseconds duration, StoryPace pace) = 0;
    virtual void wait(Milliseconds duration) = 0;
    //  The game now, once every input sent so far has been done.
    virtual Observation<PoseType> observe() = 0;
    //  A whole battle, from its start to the overworld. High level for now:
    //  a console fights with the game's battle code, a simulation with its
    //  battle model. `args` are the BATTLE step's.
    virtual Outcome battle(const StoryActionArgs& args) = 0;
    virtual void log(std::string_view line) = 0;
    virtual CancellableScope& scope() = 0;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
