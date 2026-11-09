/*  BDSP AutoStory - Talk-over
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  When a talk or a scene is over, judged from what the screen shows over
 *  time (#268's design note, slice 4): one rule for scenes, the speeches
 *  after a battle and a trainer's lines before one. It replaces the loops
 *  each of those had (settle(), settle_after_battle(), talk_or_fight()'s,
 *  confirm_battle_overworld()'s), which differed in how long the quiet had
 *  to last and whether the screen had to be still.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_TalkOver_H
#define PokemonAutomation_PokemonBDSP_AutoStory_TalkOver_H

#include <string>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  What one look at the screen showed.
struct SceneLook{
    //  A dialogue box.
    bool box = false;
    //  The overworld, as OverworldDetector reads it.
    bool overworld = false;
    //  The screen barely changed since the look before (WALK_TAP_BLOCKED_CHANGE).
    bool still = false;
    //  A fade, by fade_of(): uniform, so a dim cave floor is none.
    WarpFade fade = WarpFade::NONE;
    //  The expected map's location banner read.
    bool banner = false;
    //  A battle started (TalkOverWatch says what counts).
    bool battle = false;
};

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
//  One press is held this long,
constexpr Milliseconds TALK_OVER_HOLD = Milliseconds(80);
//  then released this long before the next look.
constexpr Milliseconds TALK_OVER_RELEASE = Milliseconds(1200);
//  How long a talk-over may take before the step fails.
constexpr Milliseconds TALK_OVER_LIMIT = Milliseconds(120000);

//  The judgement, from looks alone: tests drive it with scripted scenes.
class TalkOver{
public:
    explicit TalkOver(TalkOverRule rule = {})
        : m_rule(rule)
    {}

    SceneMove look(const SceneLook& look, WallClock now);

    //  Whether the warp the rule asks for has shown (true without one).
    bool warped() const;
    //  What it has seen, for the log: "black fade, banner read".
    std::string describe() const;

private:
    TalkOverRule m_rule;
    WarpFade m_fade = WarpFade::NONE;
    bool m_banner = false;
    WallClock m_quiet_since = WallClock::max();
    WallClock m_calm_since = WallClock::max();
};


//  What counts as a battle starting during a talk-over.
enum class TalkOverWatch{
    //  A wild battle's menu; it is fled and the talk-over goes on. A black
    //  screen is a fade (a scene that changes map).
    SCENE,
    //  A trainer's battle: its menu (Roark's read as STANDARD, r210d) or the
    //  black screen it opens with. The talk-over ends there.
    TRAINER,
};

//  How talk_over() presses and looks.
struct TalkOverSpec{
    //  Pressed while a box shows: B, or A for lines that give an item.
    Button next = BUTTON_B;
    TalkOverRule rule = {};
    TalkOverWatch watch = TalkOverWatch::SCENE;
    //  The expected map's banner name for TalkOverRule::warp ("" for a fade only).
    std::string banner = "";
};

//  How talk_over() ended.
enum class TalkOverEnd{
    QUIET,
    BATTLE,     //  Only with TalkOverWatch::TRAINER.
};

//  Presses through any text and scene on the console until TalkOver says it
//  is over. Fails with a screenshot after TALK_OVER_LIMIT.
TalkOverEnd talk_over(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec = {});



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
