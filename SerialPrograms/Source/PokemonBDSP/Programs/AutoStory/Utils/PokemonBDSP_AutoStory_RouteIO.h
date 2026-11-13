/*  BDSP AutoStory - Route IO
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the planned route walker (PlannedRoute) does on the console: walks,
 *  presses, screen reads and its log. The walker's decisions take a RouteIO,
 *  so tests can run them on a script of console outcomes; ConsoleRouteIO is
 *  the console.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteIO_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteIO_H

#include <functional>
#include <optional>
#include <string>
#include "Common/Cpp/Color.h"
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryBreakpoints.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "Pokemon/Navigation/Pokemon_RouteLogLegs.h"
#include "PokemonBDSP_AutoStory_RouteWalk.h"
#include "PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_WalkDpad.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  A walk on the D-pad toward a wall: walk_until_dpad() toward `direction`.
struct RouteWallWalk{
    Pokemon::Direction direction;
    Milliseconds limit;
    std::string label;
    DpadWalkOptions options;
};

//  What to watch for within a wait: a dialogue box (of the given color, for
//  the game's text box), and/or the location banner.
struct RouteWatch{
    Milliseconds wait;
    std::optional<Color> dialog = std::nullopt;
    bool banner = false;
};

//  What RouteWatch saw: whether the dialogue box showed, the banner read ("" if none).
struct RouteSight{
    bool dialog = false;
    std::string banner;
};

//  How a line goes to the log.
enum class RouteNote{
    LOG,
    WARNING,    //  in orange
    STATUS,     //  route_status(): also on the video overlay
    FAILURE,    //  fails the program with a screenshot; does not return
};

//  Abstract I/O interface for route traversal, logging, and controller execution.
class RouteIO{
public:
    virtual ~RouteIO() = default;

    //  Holds the D-pad toward a wall (walk_until_dpad()).
    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) = 0;

    //  Taps `tiles` whole tiles toward `direction` (walk_tiles_dpad()).
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) = 0;

    //  Presses or waits (press_route_input()).
    virtual void press(const RouteInput& input) = 0;

    //  What shows within `watch.wait`. A door's fade can go by between two
    //  frames of a walk, and the banner of the map beyond shows a second or
    //  so after it.
    virtual RouteSight sight_within(const RouteWatch& watch) = 0;

    //  Holds the stick toward `direction` for 1.6 s, and returns whether the
    //  screen changed less than walking would: we stand at a wall.
    virtual bool holding_against_wall(Pokemon::Direction direction, const std::string& label) = 0;

    //  Once the presses so far are done, shows `at` the screen, if there is one.
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>& at) = 0;

    //  Writes `text` to the log; a FAILURE fails the program.
    virtual void note(const std::string& text, RouteNote kind) = 0;

    //  A leg is about to be walked: its line to the log (note(), STATUS),
    //  then the breakpoint before it, if the run has `pause`.
    void leg_started(size_t number, size_t count, const Pokemon::PlannedRouteLeg& leg, Pokemon::StoryPausePoints* pause);
};


//  The console: today's walker steps, unchanged.
class ConsoleRouteIO : public RouteIO{
public:
    ConsoleRouteIO(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape)
        : m_stream(stream)
        , m_context(context)
        , m_escape(escape)
    {}

    virtual WalkResult walk_to_wall(const RouteWallWalk& walk) override;
    virtual WalkResult walk_taps(Pokemon::Direction direction, size_t tiles, const std::string& label) override;
    virtual void press(const RouteInput& input) override;
    virtual RouteSight sight_within(const RouteWatch& watch) override;
    virtual bool holding_against_wall(Pokemon::Direction direction, const std::string& label) override;
    virtual void look(const std::function<void(const ImageViewRGB32& screen)>& at) override;
    virtual void note(const std::string& text, RouteNote kind) override;

private:
    VideoStream& m_stream;
    ProControllerContext& m_context;
    const WildBattleEscape& m_escape;
};


//  Whether a dialogue box shows within `wait`.
inline bool dialog_within(RouteIO& io, Milliseconds wait, Color color){
    return io.sight_within({.wait = wait, .dialog = color}).dialog;
}

//  The location banner that shows within `wait`, "" if none.
inline std::string banner_within(RouteIO& io, Milliseconds wait){
    return io.sight_within({.wait = wait, .banner = true}).banner;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
