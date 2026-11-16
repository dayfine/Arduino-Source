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

//  How a walk's legs are played.
enum class StoryWalkStyle{
    //  Each turn is L, a 150 ms tap that way, L, then the stick held
    //  forward (the 2026-11-08 recording).
    TURN_THEN_WALK,
    //  The stick is held at the leg's angle for its whole time, from the
    //  camera as it is; L recenters the camera only before a later leg that
    //  turns (02.01, console B 2026-11-10). L near an NPC turns to it, so a
    //  held walk starts without one.
    HELD,
};

//  A walk in the overworld. Lumiose has no tiles: a move is legs of turns
//  and timed walks, as recorded on the console.
struct StoryMove{
    std::vector<StoryWalkLeg> legs = {};
    StoryWalkStyle style = StoryWalkStyle::TURN_THEN_WALK;
    //  A wait before the walk looks for free control, for the NPCs to move
    //  off (02.01). The walk's caller waits it; run_story_move() does not.
    Milliseconds wait_first = Milliseconds(0);
};

//  From the overworld, walks as `move` says. TURN_THEN_WALK: each leg's
//  turn (and L to recenter the camera, since the walk goes where it looks)
//  and walk, with a look after each turn and each walk; one that shows the
//  screen still (the game dropped the input) plays that turn or walk once
//  more, then goes on. HELD: each leg's stick held, unchecked. A leg cut short or blocked part way goes unnoticed, and a step
//  that needs a prompt after the walk fails there instead.
Pokemon::Outcome run_story_move(PlzaBackend& backend, const StoryMove& move);



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
