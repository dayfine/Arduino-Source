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


class StoryStepIO{
public:
    virtual ~StoryStepIO() = default;

    //  The route walker's console: walks, presses, dialogue watches, the log.
    virtual RouteIO& route() = 0;
    //  Fights a trainer whose dialogue has opened (fight_route_trainer()).
    virtual void fight(const std::string& trainer) = 0;
    //  walk_until_stopped(): `direction` until something stops us, for at
    //  most `tiles` tiles' time. Fails on a door or a dialogue.
    virtual void walk_until_stopped(Pokemon::Direction direction, size_t tiles, const std::string& label) = 0;
    //  Runs `direction` on the D-pad until the location banner of `map` (a
    //  JSON map's name) shows, for at most `limit`. Returns whether it showed.
    virtual bool run_until_map(Pokemon::Direction direction, Milliseconds limit, const std::string& map, const std::string& label) = 0;
    //  Holds the D-pad toward a door (running with `run`) until the screen
    //  goes dark or a line shows. Returns whether it went dark.
    virtual bool run_into_door(Pokemon::Direction direction, bool run) = 0;
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
    virtual bool run_into_door(Pokemon::Direction direction, bool run) override;
    virtual void settle(Button button, const std::string& label, bool fade) override;

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    WildBattleEscape m_escape;
    ConsoleRouteIO m_route;
};



}
}
}
#endif
