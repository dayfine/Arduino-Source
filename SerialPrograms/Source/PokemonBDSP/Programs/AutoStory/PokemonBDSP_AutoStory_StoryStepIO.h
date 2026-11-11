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
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkResult.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_WalkScreen.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  How a door run takes a warp's fade (WarpFadeWatch), on the console and
//  the simulation alike: the partial tier's dark (a doorway's dim fade
//  frames) held 50 ms, white held 60 ms, as its black and white watchers
//  did (lab-B to 2026-11-08), or a fade whose hold it did not see, at the
//  other scene after the fall.
constexpr WarpFadeRule DOOR_FADE_RULE{.partial_dark = true, .dark_for = Milliseconds(50), .white_for = Milliseconds(60)};
//  How a tap step, looking every 100 ms, takes one: the uniform tier at
//  once, as fade_of() did, or a fade whose hold it did not see.
constexpr WarpFadeRule TAP_FADE_RULE{};


//  How a settle (StoryStepIO::settle()) ends.
enum class SettleKind{
    //  Text and scenes pressed through, then the overworld quiet.
    TALK,
    //  As TALK, but a warp (a map change) must come first: a fade, or the
    //  far map's banner.
    AFTER_WARP,
    //  After a door or a way through with no text to come: the fade back
    //  up and a short still spell around the player, people walking about
    //  ignored (TalkOverRule::door).
    DOOR,
};


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
    //  its fade (DOOR_FADE_RULE), `banner` (a map's banner name, "" for
    //  none) reads, or a line shows. A dark screen in the first `dark_from`
    //  of running is not the door's fade, and a wild battle on the way is
    //  fled, the run going on for the time left. Returns what it saw of the
    //  warp: the fade or the banner, either one alone (#268 slice 1).
    virtual WarpSeen run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000),
        Milliseconds dark_from = Milliseconds(0), const std::string& banner = "") = 0;
    //  D-pad taps `direction` one at a time, each looked at, toward a door
    //  or cave mouth, until its fade (TAP_FADE_RULE: uniform, so a dim cave
    //  floor is none) or the location banner of `map` (a JSON map's name), for at
    //  most `max_taps`; after a fade, no more taps. Either one alone is the
    //  way through. The watch starts before the first tap: a banner up then
    //  is 0 taps. Returns the taps it took, or nothing if neither showed.
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) = 0;
    //  Presses through any text until the overworld ends as `kind` says;
    //  for SettleKind::AFTER_WARP, the warp is a fade or the banner `banner`
    //  ("" for a fade only). Fails otherwise.
    virtual void settle(Button button, const std::string& label, SettleKind kind, const std::string& banner) = 0;
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
    virtual WarpSeen run_into_door(Pokemon::Direction direction, bool run, Milliseconds timeout = Milliseconds(12000),
        Milliseconds dark_from = Milliseconds(0), const std::string& banner = "") override;
    virtual std::optional<size_t> tap_to_banner(Pokemon::Direction direction, const std::string& map, size_t max_taps) override;
    virtual void settle(Button button, const std::string& label, SettleKind kind, const std::string& banner) override;


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
