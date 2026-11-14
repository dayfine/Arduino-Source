/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's walks in the overworld (PokemonLZA_AutoStoryActions.h has
 *  the other actions): recorded turns and timed walks, the compass, a
 *  fast travel.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMoves_H
#define PokemonAutomation_PokemonLZA_AutoStoryMoves_H

#include <optional>
#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
#include "Pokemon/AutoStory/Types/Pokemon_StoryBreakpoints.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryBackend.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  One leg of a walk: a turn, then a walk straight on.
struct StoryWalkLeg{
    //  Degrees clockwise from where the player faces (negative: left), as
    //  the story was recorded: L recenters the camera behind the player,
    //  the stick is tapped that way for 150 ms, and L again. The tap also
    //  takes a step. 0: no turn.
    double turn = 0;
    //  How long to hold the stick forward after the turn, walking (not
    //  dashing). 0: the turn only.
    Milliseconds walk = Milliseconds(0);
};

//  A walk in the overworld. Lumiose has no tiles: a move is an optional
//  compass heading to face first, then legs of turns and timed walks, as
//  recorded on the console.
struct StoryMove{
    //  Clockwise from north, as the minimap's arrow reads it
    //  (get_facing_direction()). None: no compass turn first.
    std::optional<double> heading = std::nullopt;
    std::vector<StoryWalkLeg> legs = {};
    //  How the legs are played. False: each turn is L, a 150 ms tap that
    //  way, L, then the stick held forward (the 2026-11-08 recording).
    //  True: the stick is held at the leg's angle for its whole time, from
    //  the camera as it is; L recenters the camera only before a later leg
    //  that turns (02.01, console B 2026-11-10). L near an NPC turns to it,
    //  so a held walk starts without one.
    bool held = false;
    //  A wait before the walk looks for free control: for the NPCs to move
    //  off (02.01), or for a ride's fade to start (02.05's elevator). The
    //  walk's caller waits it; run_story_move() does not.
    Milliseconds wait_first = Milliseconds(0);
};

//  Turns the player toward `heading`, within `tolerance` degrees: turns by
//  the difference (story_turn()) and reads the heading again, a few times.
//  The heading is a look's STORY_HEADING reading, filled in by `reader`
//  (heading_reader() on a console; none where observe() sets it). Fails as
//  "heading_unread" when no look has it and "heading_missed" if it stays
//  outside the tolerance.
Pokemon::Outcome face_heading(PlzaBackend& backend, const PlzaReader& reader, double heading, double tolerance);

//  From the overworld, walks as `move` says: faces its heading first, if it
//  has one, then each leg's turn (and L to recenter the camera, since the
//  walk goes where it looks) and walk. Nothing is read on the way: a leg
//  that is cut short or blocked goes unnoticed, and a step that needs a
//  prompt after the walk fails there instead. `heading` reads the compass
//  (face_heading()). `pause`, if given, is the run's pause points: each leg
//  is a breakpoint (StoryPausePoints::before_leg(), story_walk_leg_label()).
Pokemon::Outcome run_story_move(
    PlzaBackend& backend, const StoryMove& move, const PlzaReader& heading,
    Pokemon::StoryPausePoints* pause = nullptr
);

//  "leg 2/5 turn 90 walk 1200 ms": leg `index` (from 0) of `count`, as a
//  breakpoint's label. A leg with no walk leaves it out.
std::string story_walk_leg_label(size_t index, size_t count, const StoryWalkLeg& leg);

//  From the overworld, opens the map and fast travels to `location`
//  (open_map_and_fly_to()). Returns in the overworld there; throws
//  OperationFailedException if the game would not fly (pursued, or the
//  place is not unlocked).
void fly_to(ConsoleHandle& console, ProControllerContext& context, Language language, Location location);

//  fly_to(), where not flying is no failure: when the game would not fly
//  (fast travel not unlocked yet, pursued, the place not listed), logs it,
//  backs out of the map to the overworld and returns false; the player is
//  where it was. A map with no Travel Spots icon (fast travel not open)
//  is found in 5 s, not open_map()'s 30. Returns true in the overworld at
//  `location`.
bool fly_to_or_stay(ConsoleHandle& console, ProControllerContext& context, Language language, Location location);

//  03.08's start: from the Pokemon Center (03.07d walks back
//  there; fast travel is not open yet at 03.07's end, console B
//  2026-11-13), on to Wild Zone 1's entrance gate by the objective marker
//  (walk_to_marker()), A at its "A Enter", B to the overworld. Where the
//  player is in the zone already (starts_in_zone_1(): a run straight on
//  from 03.07), nothing. UNVERIFIED, NEEDS
//  CONSOLE: that the marker's way from the Center leads to the entrance
//  gate's prompt. Throws OperationFailedException when no prompt shows.
void center_to_zone_1(ConsoleHandle& console, ProControllerContext& context);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
