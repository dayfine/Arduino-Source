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
#include "Pokemon/AutoStory/Pokemon_StoryOutcome.h"
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

//  A walk in the overworld. Lumiose has no tiles: a move is legs of turns
//  and timed walks, as recorded on the console.
struct StoryMove{
    std::vector<StoryWalkLeg> legs = {};
};

//  From the overworld, walks as `move` says: each leg's turn (and L to
//  recenter the camera, since the walk goes where it looks) and walk. A
//  look after each turn and each walk; one that shows the screen still
//  (the game dropped the input) plays that turn or walk once more, then
//  goes on. A leg cut short or blocked part way goes unnoticed, and a step
//  that needs a prompt after the walk fails there instead.
Pokemon::Outcome run_story_move(PlzaBackend& backend, const StoryMove& move);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
