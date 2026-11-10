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
#include <vector>
#include "Common/Cpp/Time.h"
#include "CommonFramework/Language.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "VideoGames/PokemonLZA/Resources/PokemonLZA_Locations.h"

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

//  The heading the player faces, clockwise from north in [0, 360): the
//  minimap's arrow (get_facing_direction()).
double read_heading(ConsoleHandle& console, ProControllerContext& context);

//  Turns the player toward `heading`, within `tolerance` degrees: recenters
//  the camera behind the player, then taps the stick toward it and reads the
//  minimap again, a few times. Returns the heading read last; throws
//  OperationFailedException if it stays outside the tolerance.
double face_heading(ConsoleHandle& console, ProControllerContext& context, double heading, double tolerance);

//  From the overworld, walks as `move` says: faces its heading first, if it
//  has one, then each leg's turn and walk. Dash is a toggle in this game;
//  the walk assumes it is off. Nothing is read on the way: a leg that is cut
//  short or blocked goes unnoticed, and a step that needs a prompt after the
//  walk fails there instead.
void run_story_move(ConsoleHandle& console, ProControllerContext& context, const StoryMove& move);

//  From the overworld, opens the map and fast travels to `location`
//  (open_map_and_fly_to()). Returns in the overworld there; throws
//  OperationFailedException if the game would not fly (pursued, or the
//  place is not unlocked).
void fly_to(ConsoleHandle& console, ProControllerContext& context, Language language, Location location);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
