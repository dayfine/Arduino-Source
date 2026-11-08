/*  BDSP AutoStory - Story step IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story's walking steps (StoryWalkSteps) do on the console beyond
 *  the route walker's RouteIO: fights, walks until stopped, runs into doors
 *  and the settle after a scene. Tests run the steps on a simulation;
 *  ConsoleStoryStepIO is the console.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_StoryStepIO_H
#define PokemonAutomation_PokemonBDSP_AutoStory_StoryStepIO_H

#include <optional>
#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/NintendoSwitch_ControllerButtons.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_RouteIO.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Abstract interface for story step input/output operations.
class StoryStepIO{
public:
    virtual ~StoryStepIO() = default;

    //  The route walker's console: walks, presses, dialogue watches, the log.
    virtual RouteIO& route() = 0;
    //  Fights a trainer whose dialogue has opened (fight_route_trainer()):
    //  Route 204's Twins as a pair (01.10), everyone else alone.
    virtual void fight(const std::string& trainer) = 0;
    //  walk_until_stopped(): `direction` until something stops us, for at
    //  most `tiles` tiles' time. Fails on a door or a dialogue.
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) = 0;
    //  Runs `direction` on the D-pad until the location banner of `map` (a
    //  JSON map's name) shows, for at most `limit`. Returns whether it showed.
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string& label) = 0;
    //  Holds the D-pad toward a door (running with `run`) for `timeout` until
    //  the screen goes dark or a line shows. A dark screen in the first
    //  `dark_from` of running is not the door's fade, and a wild battle on the
    //  way is fled, the run going on for the time left. Returns whether it went dark.
    virtual bool run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000), Milliseconds dark_from = Milliseconds(0)) = 0;
    //  D-pad taps `direction` one at a time, each looked at, toward a door
    //  or cave mouth, until the fade (then no more taps) and the location
    //  banner of `map` (a JSON map's name), for at most `max_taps`. Returns
    //  the taps it took, or nothing if no banner showed.
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) = 0;
    //  Presses through any text until the overworld is quiet; with `fade`, a
    //  fade must come first (a map change). Fails otherwise.
    virtual void settle(Button button, const std::string& label, bool fade) = 0;
};


//  The console.
class ConsoleStoryStepIO : public StoryStepIO{
public:
    ConsoleStoryStepIO(VideoStream& stream, ProControllerContext& context);

    virtual RouteIO& route() override{
        return m_route;
    }
    virtual void fight(const std::string& trainer) override;
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string& label) override;
    virtual bool run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000), Milliseconds dark_from = Milliseconds(0)) override;
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) override;
    virtual void settle(Button button, const std::string& label, bool fade) override;

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    WildBattleEscape m_escape;
    ConsoleRouteIO m_route;
};



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
