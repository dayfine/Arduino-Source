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



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
