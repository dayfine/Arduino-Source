/*  AutoStory Story Start Gate
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  How a story run may start: the ways to reach the start checkpoint, the
 *  console checks before anything is pressed, and whether the first
 *  checkpoint may save at its start. The rules are in Pokemon_StoryRun.h.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryStartGate_H
#define PokemonAutomation_Pokemon_StoryStartGate_H

#include <string_view>
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  How the game reaches the start checkpoint.
enum class StoryStart{
    //  A new game from the title screen: there is no save.
    NEW_GAME,
    //  The game as another program left it: nothing is loaded.
    LIVE,
    //  The checkpoint's save, loaded first.
    FROM_SAVE,
};

//  What a game reads off the console before a run presses anything.
struct StoryPreflight{
    //  Frames are arriving and show a picture (not black, not color bars).
    bool video_alive = false;
    bool controller_ready = false;
    //  The game is on screen, not the Switch's HOME menu.
    bool game_running = false;
};

//  The failure of a preflight that failed: the console or its setup, not
//  the story. Nothing answers it with a reload or a recovery.
inline constexpr std::string_view STORY_PRECONDITION_FAILED = "precondition_failed";

//  outcome_ok() when `preflight` passed, else STORY_PRECONDITION_FAILED
//  naming each check that failed.
Outcome check_story_preflight(const StoryPreflight& preflight);

//  Whether the first checkpoint's own save at its start may run, for a run
//  that starts as `start` says: only a new game's (see Pokemon_StoryRun.h).
bool story_first_save_allowed(StoryStart start);

//  "its save", "a new game", "the game as it is": for the start line.
std::string_view story_start_name(StoryStart start);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
