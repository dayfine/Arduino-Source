/*  PokemonLZA AutoStory - Moves
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story's walks in the overworld: recorded turns and timed walks.
 *
 */

#ifndef PokemonAutomation_PokemonLZA_AutoStoryMoves_H
#define PokemonAutomation_PokemonLZA_AutoStoryMoves_H

#include <string>
#include <vector>
#include "Common/Cpp/Time.h"
#include "Pokemon/AutoStory/Pokemon_StoryFailure.h"
#include "VideoGames/PokemonLZA/Programs/AutoStory/PokemonLZA_AutoStoryTypes.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


//  One leg of a walk: a turn, then a walk straight on.
struct StoryWalkLeg{
    //  Degrees clockwise from where the player faces. L recenters the camera,
    //  the stick is tapped that way for 150 ms (which also takes a step), and L
    //  again.
    double turn = 0;

    //  Walking, not dashing. 0: the turn only.
    Milliseconds walk = Milliseconds(0);
};

//  How a walk's legs are played.
enum class StoryWalkStyle{
    TURN_THEN_WALK,
    //  The stick held at the leg's angle from the camera as it is; L only
    //  before a later leg that turns. L near an NPC turns to it.
    HELD,
};

//  Lumiose has no tiles: a move is legs of turns and timed walks.
struct StoryMove{
    std::vector<StoryWalkLeg> legs = {};
    StoryWalkStyle style = StoryWalkStyle::TURN_THEN_WALK;
    //  For the NPCs to move off; the caller waits it, run_story_move() does not.
    Milliseconds wait_first = Milliseconds(0);
};

//  TURN_THEN_WALK: a look after each turn and walk; one that shows the
//  screen still (a dropped input) plays it once more. HELD: unchecked. A
//  leg cut short goes unnoticed; the step after it fails instead.
std::optional<Pokemon::StoryFailure> run_story_move(PlzaBackend& backend, const StoryMove& move);

//  All but the last leg's walk, which the caller holds a slice at a time
//  (hold_last_leg()). A HELD last leg with no walk is played whole.
std::optional<Pokemon::StoryFailure> run_story_move_but_last_walk(PlzaBackend& backend, const StoryMove& move);

//  `slice` of `move`'s last leg's walk.
void hold_last_leg(PlzaBackend& backend, const StoryMove& move, Milliseconds slice);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
