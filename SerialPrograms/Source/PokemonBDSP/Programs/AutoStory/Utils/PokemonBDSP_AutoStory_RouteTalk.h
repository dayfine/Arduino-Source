/*  BDSP AutoStory - Route talk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Talking to someone next to us, and finishing a dialogue.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteTalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteTalk_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "Pokemon/Navigation/Pokemon_Grid.h"
#include "PokemonBDSP_AutoStory_WalkResult.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Talks to someone on a tile next to ours, without trusting the tile we
//  counted ourselves onto: for each of `directions` in order, turns that way,
//  presses A and waits up to `wait` for a dialogue. Returns the direction that
//  answered, its dialogue open, or none if nobody did. Turns are taps (see
//  route_turn()), so do not list the way the player already faces first.
std::optional<Pokemon::Direction> talk_to_adjacent_npc(
    VideoStream& stream, ProControllerContext& context,
    const std::vector<Pokemon::Direction>& directions,
    const std::string& label, Milliseconds wait = Milliseconds(1500)
);

//  Talks to someone straight ahead in `facing`, where we may stand a little
//  off their tile: BDSP moves by any distance, and a sideways tap moves about
//  a third of a tile (a 50ms tap 0.2 to 0.45 tiles, a 100ms turn tap 0.4, on
//  the console 2026-10-31). Presses A, and while nobody answers shifts by one
//  such tap at a time to -1, +1, -2, +2 taps from the start and tries again.
//  Never turns away and back between tries: each turn is a tap that moves us
//  (s03e2 drifted east of Roark a third of a tile per try). Returns whether a
//  dialogue opened, left open; false also after a wild battle on a nudge,
//  which leaves us off the place we were.
//  `accept`, if given, looks at the screen before A and once the dialogue
//  is open, and says whether it is the one wanted: a rock or a sign next to
//  someone answers A too (r187a read "It's a rugged rock" beside Roark). A
//  dialogue it turns down is closed with B and the nudges go on.
bool talk_with_nudges(
    VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape,
    Pokemon::Direction facing, const std::string& label,
    Milliseconds wait = Milliseconds(3000),
    const std::function<bool(const ImageViewRGB32& before, const ImageViewRGB32& after)>& accept = nullptr
);
//  D-pad nudge tap duration for micro-adjusting player position.
constexpr Milliseconds ROUTE_NUDGE_TAP = Milliseconds(50);

//  Finishes a dialogue that has opened, however many lines it has: presses
//  `button` while a dialogue box shows, and returns once the overworld has
//  shown with no box for 1.5 s (the box blinks off between lines and during
//  "obtained" jingles). Use A where the talk asks Yes/No or gives an item (A
//  answers Yes and closes "obtained" lines; B would answer No), B elsewhere
//  (B never starts a new talk). Fails with a screenshot after `timeout`.
void finish_dialogue(
    VideoStream& stream, ProControllerContext& context,
    Button button, const std::string& label,
    Milliseconds timeout = std::chrono::seconds(40)
);


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
