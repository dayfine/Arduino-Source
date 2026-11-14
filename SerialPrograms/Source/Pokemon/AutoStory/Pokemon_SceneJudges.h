/*  Scene Judges
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Judgements a story step makes from screen events (Pokemon_ScreenEvents.h)
 *  over time, not from one look (#268's design note): a warp is a fade or
 *  the banner of the map arrived on, and a talk or a scene is over when the
 *  overworld has been quiet for a while. Pure: tests drive them with
 *  scripted scenes, and every game shares them.
 *
 */

#ifndef PokemonAutomation_Pokemon_SceneJudges_H
#define PokemonAutomation_Pokemon_SceneJudges_H

#include <string>
#include "Common/Cpp/Time.h"
#include "Pokemon_ScreenEvents.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a walk or a step saw of a warp (#268): a fade or the location
//  banner of the map arrived on. Either one alone is the warp.
struct WarpSeen{
    WarpFade fade = WarpFade::NONE;
    //  The banner read, "" if none.
    std::string banner = {};

    bool any() const{
        return fade != WarpFade::NONE || !banner.empty();
    }
};
//  `seen` after a frame's events: their fade, and `banner_read` when they
//  show LOCATION_BANNER. The first of each counts.
WarpSeen note_warp(WarpSeen seen, const ScreenEvents& events, const std::string& banner_read);
//  How it was seen, for the log: a black fade, banner "Route 203".
std::string describe(const WarpSeen& seen);


//  What to do after a look.
enum class SceneMove{
    NEXT,       //  A box shows: press the talk's button.
    B,          //  No box: press B. It never starts a talk on the overworld
                //  (A would, and answers Yes on a prompt).
    BATTLE,     //  A battle started.
    DONE,       //  The talk or scene is over.
};

//  What ends a talk-over, besides the quiet.
struct TalkOverRule{
    //  A map change must come first: a fade or the banner. Only the quiet
    //  after it counts: r30's scene went quiet for 5 s before the fade home.
    bool warp = false;
    //  False when a battle is to follow (a trainer's lines pause for more
    //  than 4 s before it, r210d): no quiet ends the talk then.
    bool end_on_quiet = true;
    //  The settle after a door or a warp, with no text to come: only the
    //  fade back up and a short still spell around the player count, so
    //  people walking about (a Center's visitors) neither hold it up nor
    //  end it early. DOOR_QUIET and DOOR_CALM, not the talk's 5 s and 15 s.
    bool door = false;

    //  The box around the player still this long after the fade is back
    //  up ends a door's settle; a walk any earlier could still go nowhere
    //  in the fade-in (lab-B 2026-11-10, 00.04b's nurse).
    static constexpr Milliseconds DOOR_QUIET = Milliseconds(1000);
    //  The overworld back up this long ends it, still or not (someone
    //  walking past the player the whole time).
    static constexpr Milliseconds DOOR_CALM = Milliseconds(2500);
};

//  The overworld with no box and a still screen this long ends it: a scene
//  pauses while someone walks off (Dawn and Barry after Rowan leaves, r23),
//  a scripted walk-in ran 5 s with no text before Mom spoke (r31), and
//  Roark's badge speech pauses for the fanfare (lab-B 2026-10-31 09:45 and
//  2026-11-08 15:06). The clock is the last look that was not quiet.
constexpr Milliseconds TALK_OVER_QUIET = Milliseconds(5000);
//  The overworld with no box this long ends it, still or not: wanderers or
//  grass can keep the screen from ever being still.
constexpr Milliseconds TALK_OVER_CALM = Milliseconds(15000);

//  When a talk or a scene is over, from looks alone. A look is a frame's
//  events: DIALOG_BOX, PLAYER_CAN_MOVE, SCREEN_STILL (PLAYER_STILL for a
//  door's settle), a fade,
//  LOCATION_BANNER (the expected map's) and BATTLE_STARTED are read.
//  It is over only once every fade has come back up, no box shows, and
//  the overworld has been quiet (or calm) since: a look that shows a fade
//  starts the quiet over, however still it is.
class TalkOver{
public:
    explicit TalkOver(TalkOverRule rule = {})
        : m_rule(rule)
    {}

    SceneMove look(const ScreenEvents& look, WallClock now);

    //  Whether the warp the rule asks for has shown (true without one).
    bool warped() const;
    //  What it has seen, for the log: "black fade, banner read".
    std::string describe() const;

private:
    TalkOverRule m_rule;
    //  The first fade seen, and the one the last look showed.
    WarpFade m_fade = WarpFade::NONE;
    WarpFade m_fade_showing = WarpFade::NONE;
    bool m_banner = false;
    WallClock m_quiet_since = WallClock::max();
    WallClock m_calm_since = WallClock::max();
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
