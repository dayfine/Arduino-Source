/*  Settle
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a story step waits for the game to settle after a talk, a scene, a
 *  warp or a door: one loop over TalkOver (Pokemon_SceneJudges.h) for
 *  every game, the console and the sims alike. A game gives it the looks
 *  and the presses through SettleIO; the rule for each kind of settle is
 *  one table here.
 *
 */

#ifndef PokemonAutomation_Pokemon_Settle_H
#define PokemonAutomation_Pokemon_Settle_H

#include <string>
#include <string_view>
#include "Common/Cpp/Time.h"
#include "CommonTools/ScreenEvents/ScreenEventTypes.h"
#include "Pokemon_SceneJudges.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What ends a settle.
enum class SettleKind{
    //  Text and scenes pressed through, then the overworld quiet.
    TALK,
    //  As TALK, but a warp (a map change) must come first: a fade, or the
    //  far map's banner.
    AFTER_WARP,
    //  After a door or a way through with no text to come: the fade back
    //  up and a short still spell around the player (PLAYER_STILL), people
    //  walking about ignored.
    DOOR,
};

//  A door's settle: the player still this long after the fade is back up
//  ends it; a walk any earlier could still go nowhere in the fade-in
//  (BDSP lab-B 2026-11-10, 00.04b's nurse).
inline constexpr Milliseconds DOOR_QUIET = Milliseconds(1000);
//  A door's settle: the overworld back up this long ends it, still or not
//  (someone walking past the player the whole time).
inline constexpr Milliseconds DOOR_CALM = Milliseconds(2500);

//  How a settle looks and how long it may take.
struct SettleRule{
    //  What ends it.
    TalkOverRule judge = {};
    //  The wait after a press before the next look: a line's text takes
    //  about a second to run out; a door has no text and looks often to
    //  catch its short still spell.
    Milliseconds look_every = Milliseconds(1200);
    //  How long it may take before it fails.
    Milliseconds cap = Milliseconds(120000);
};

//  The rule of `kind`.
SettleRule settle_rule(SettleKind kind);


//  One look at the screen: its events, and when it was taken.
struct SettleLook{
    ScreenEvents events;
    WallClock when;
};

//  What run_settle() needs of a game: its looks and its presses.
class SettleIO{
public:
    virtual ~SettleIO() = default;

    //  The screen now.
    virtual SettleLook look() = 0;
    //  Answers a look (SceneMove::NEXT: the talk's button; SceneMove::B:
    //  B), then waits `wait`. A game that only watches waits.
    virtual void press(SceneMove move, Milliseconds wait) = 0;
    //  A battle started: flees a wild one and returns true, or returns
    //  false for one that ends the settle (a trainer's after their talk).
    virtual bool flee_battle() = 0;
    virtual void log(std::string_view line) = 0;
};

//  How a settle ended.
enum class SettleResult{
    QUIET,      //  The rule's end: the overworld quiet (or calm).
    BATTLE,     //  A battle the IO did not flee.
    TIMED_OUT,  //  The cap: the step fails.
};
struct SettleEnd{
    SettleResult result;
    //  What it saw ("black fade back up"); for TIMED_OUT, the failure.
    std::string text;
};

//  Looks and presses until `rule` says the game has settled, a battle
//  ends it, or the cap. `label` names the step in the log and the failure.
SettleEnd run_settle(SettleIO& io, const SettleRule& rule, std::string_view label);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
