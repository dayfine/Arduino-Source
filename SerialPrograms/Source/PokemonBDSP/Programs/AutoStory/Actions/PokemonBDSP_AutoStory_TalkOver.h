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
#include "Pokemon/AutoStory/Pokemon_SceneJudges.h"
#include "PokemonBDSP/Programs/AutoStory/Utils/PokemonBDSP_AutoStory_Walk.h"
#include "PokemonBDSP_AutoStory_TalkOverTally.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  The judgement is shared (Pokemon_SceneJudges.h): it reads the screen
//  events BdspScreenObserver reports.
//  What to do after a look.
using Pokemon::SceneMove;
//  What ends a talk-over, besides the quiet.
using Pokemon::TalkOverRule;
//  When a talk or a scene is over, from looks alone.
using Pokemon::TalkOver;

//  One press is held this long,
constexpr Milliseconds TALK_OVER_HOLD = Milliseconds(80);
//  then released this long before the next look.
constexpr Milliseconds TALK_OVER_RELEASE = Milliseconds(1200);
//  A door's settle (TalkOverRule::door) has no text to wait out: it looks
//  this often, to see its short still spell when it comes.
constexpr Milliseconds TALK_OVER_DOOR_RELEASE = Milliseconds(200);
//  The release before the next look under `rule`.
inline Milliseconds talk_over_release(const TalkOverRule& rule){
    return rule.door ? TALK_OVER_DOOR_RELEASE : TALK_OVER_RELEASE;
}

//  How long a talk-over may take before the step fails.
constexpr Milliseconds TALK_OVER_LIMIT = Milliseconds(120000);


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

//  Presses through any text and scene on the console until TalkOver says it
//  is over. Fails with a screenshot after TALK_OVER_LIMIT.
TalkOverSeen talk_over_seen(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec = {});
//  talk_over_seen() without the count.
TalkOverEnd talk_over(VideoStream& stream, ProControllerContext& context, const std::string& label, const TalkOverSpec& spec = {});



} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
