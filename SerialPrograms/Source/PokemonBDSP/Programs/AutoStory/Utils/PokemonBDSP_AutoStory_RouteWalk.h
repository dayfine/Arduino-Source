/*  BDSP AutoStory - Route Walk
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What walking on the story maps is made of: walking times per tile, the
 *  stick for a direction, turning, talking to someone next to us, finishing
 *  a dialogue, and going through doors. PlannedRoute walks whole plans.
 *
 */

#ifndef PokemonAutomation_PokemonBDSP_AutoStory_RouteWalk_H
#define PokemonAutomation_PokemonBDSP_AutoStory_RouteWalk_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/ImageTypes/ImageViewRGB32.h"
#include "CommonFramework/Tools/VideoStream.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "PokemonBDSP/Programs/AutoStory/Data/PokemonBDSP_AutoStory_StoryWorld.h"
#include "PokemonBDSP_AutoStory_Walk.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


//  Walking time per tile, measured on the console from the post-catch save:
//  a row is deeper than a column is wide under the tilted camera.
constexpr Milliseconds ROUTE_MS_PER_TILE_ACROSS = Milliseconds(160);    //  east or west
constexpr Milliseconds ROUTE_MS_PER_TILE_UP_DOWN = Milliseconds(240);   //  north or south
//  From pressing the stick to the first step, when standing still.
constexpr Milliseconds ROUTE_START_DELAY = Milliseconds(250);
//  Running (B held) covers a tile in this fraction of the walking time. B
//  held with the D-pad did not run: r238a walked the Ravaged Path's corridor
//  at 160 ms a tile, and a limit set for twice that speed cut a wall leg
//  short by two tiles. 1 until running is measured to work.
constexpr int ROUTE_RUN_SPEEDUP = 1;

//  Walking time per tile in `direction`.
Milliseconds route_ms_per_tile(Pokemon::Direction direction);
//  The left stick for a direction. Row 0 is the top of a map, and the
//  stick's y is positive upward.
JoystickPosition route_stick(Pokemon::Direction direction);

//  How long to hold the stick for an open leg of `tiles` tiles, for a player
//  already facing that way: a step once started is always finished, so stop
//  half a tile early. On the console a 1290ms hold west went 9 tiles and an
//  800ms hold south 4: the player starts at once and finishes the step.
Milliseconds route_open_hold(Pokemon::Direction direction, size_t tiles);

//  A tap this short turns a player who faces another way, without a step: on
//  the console a 150ms tap south after walking north only turned the player.
constexpr Milliseconds ROUTE_TURN_TAP = Milliseconds(100);

//  Turns the player to face `direction` without moving. Only for a player who
//  faces another way: one already facing it may take a step.
void route_turn(VideoStream& stream, ProControllerContext& context, Pokemon::Direction direction);

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
constexpr Milliseconds ROUTE_NUDGE_TAP = Milliseconds(50);

//  Whether a walk to the wall that ends on any of `after` can tell it has
//  stopped: not in tall grass, whose sway keeps the screen changing at a
//  wall. Elsewhere, a walk to the wall that stops confirms where it ended.
bool route_wall_readable(const StoryWorld& grid, const std::vector<Pokemon::TilePos>& after);

//  Shows what the route walk is doing in the log and the video overlay.
void route_status(VideoStream& stream, const std::string& text);

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

//  For a player on the tile below a door: walks north through it. If a wall
//  stops the walk (the door frame, a tile off the opening), steps a tile east,
//  then west, and tries again. Fails with a screenshot if no step goes in.
void enter_door_north(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label);

//  For a player inside a building whose door is straight south: closes any
//  talk still up and walks south out of it, stopping at the door's fade or
//  the first look that is not a room, so it ends a tile or so outside. Out is
//  then three standing looks in a row that are not a room: one look of a lit
//  Center can miss its black edge (r259). Fails with a screenshot if five
//  walks south get nowhere.
void leave_door_south(VideoStream& stream, ProControllerContext& context, const WildBattleEscape& escape, const std::string& label);

//  Brightness (mean r + g + b) below which a look is the door's fade, and
//  above which a look that is not a room is outside.
inline constexpr double DOOR_FADE_DARK = 120.0;
inline constexpr double DOOR_OUTSIDE_BRIGHT = 200.0;

//  What leave_door_south() has seen of the door's fade so far.
struct DoorFadeWatch{
    bool saw_dark = false;
};
//  Takes one look (its brightness, and whether indoors() read a room) and
//  returns whether we are out: a dark look seen, and now a bright one that
//  is not a room.
bool door_fade_look(DoorFadeWatch& watch, double brightness, bool room);
//  Whether leave_door_south()'s walk stops at this look: at the door's fade
//  out (door_fade_look()), or at the first look that is not a room, so a
//  held stick does not walk on outside (r261 three rows).
bool door_walk_stops(DoorFadeWatch& watch, double brightness, bool room);



}
}
}
#endif
