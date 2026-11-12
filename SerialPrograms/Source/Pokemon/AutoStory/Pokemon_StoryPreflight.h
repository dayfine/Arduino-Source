/*  AutoStory Story Preflight
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  What the story run's preflight hook (StoryGameHooks::preflight) reads
 *  off the console before anything is pressed, and its verdict. The same
 *  for every game, so BDSP and FRLG lend the same reader. The start
 *  sequence that runs it is in Pokemon_StoryRun.h.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryPreflight_H
#define PokemonAutomation_Pokemon_StoryPreflight_H

#include <string_view>
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "Pokemon_StoryOutcome.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a game reads off the console before a run presses anything.
struct StoryPreflight{
    //  The failure of a preflight that failed: the console or its setup,
    //  not the story. Nothing answers it with a reload or a recovery.
    static constexpr std::string_view FAILURE = "precondition_failed";

    //  Frames are arriving and show a picture (not black, not color bars).
    bool video_alive = false;
    bool controller_ready = false;
    //  The game is on screen, not the Switch's HOME menu.
    bool game_running = false;

    //  outcome_ok() when every check passed, else FAILURE naming each
    //  check that failed.
    Outcome check() const;

    //  Reads the console: a frame newer than two seconds that shows a
    //  picture (not black, not the capture card's color bars), the
    //  controller ready, and that frame not the HOME menu. Presses nothing.
    static StoryPreflight read(NintendoSwitch::ConsoleHandle& console, NintendoSwitch::ProControllerContext& context);
};



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
