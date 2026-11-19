/*  Story Backend
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story engine's input and observation: a console, a simulation or a
 *  scripted fake.
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
#include "Pokemon_StoryPoseConcept.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How fast a held direction moves the player.
enum class StoryPace{
    WALK,
    RUN,
};

//  A console_only action (StoryActionSpec) runs only on CONSOLE.
enum class StoryBackendKind{
    CONSOLE,
    SIMULATION,
    SCRIPT,     //  a scripted fake, for unit tests
};

//  "console", "simulation", "script".
std::string_view story_backend_kind_name(StoryBackendKind kind);

//  One look at the game, with the player's pose when a reader knows it.
template <StoryPose PoseType>
struct Observation : public ScreenObservation{
    std::optional<PoseType> pose = std::nullopt;
};

//  code-health: allow public_methods -- the agreed backend boundary is these low-level calls; splitting them would make every action take two objects
template <StoryPose PoseType>
class StoryBackend{
public:
    virtual ~StoryBackend() = default;

    virtual StoryBackendKind kind() const = 0;

    virtual void press(NintendoSwitch::Button button, Milliseconds hold, Milliseconds release) = 0;
    //  The move stick held at `degrees` clockwise from north (straight on,
    //  for a camera-relative game) for `duration`, then let go for `release`
    //  (a turn's time to finish) like press(): the games' walk and turn.
    virtual void stick(double degrees, Milliseconds duration, Milliseconds release, StoryPace pace) = 0;
    virtual void wait(Milliseconds duration) = 0;
    //  The game now, once every input sent so far has been done.
    virtual Observation<PoseType> observe() = 0;
    //  Fights the battle on screen until it ends; a lost or endless battle
    //  is a failed Outcome.
    virtual Outcome battle(const StoryActionArgs& args) = 0;
    virtual void log(std::string_view line) = 0;
    //  A line for the file log only, not the stream's overlay.
    virtual void log_detail(std::string_view line){
        log(line);
    }
    virtual CancellableScope& scope() = 0;
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
