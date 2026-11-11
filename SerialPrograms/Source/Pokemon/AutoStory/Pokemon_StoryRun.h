/*  AutoStory Story Run
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The start and finish of a story run: get the
 *  game to the start checkpoint, lend the segment runner its reload, and
 *  finish a run or a verify run with the same log lines, stats and errors.
 *  A game lends only what really differs: how it loads its save (BDSP
 *  resets from HOME, FRLG soft-resets) and how it checks where the save is.
 *
 *  These rules hold for every game:
 *  -   Before anything is pressed, the game's preflight must see the video
 *      alive, the controller ready and the game running (not HOME). One
 *      that fails is the setup's failure, not the story's: nothing is
 *      touched.
 *  -   A run that starts at a checkpoint with a save loads that save first,
 *      so it never walks from wherever the game was left. There is no
 *      "just loaded" shortcut: a load or place check that fails stops the
 *      run before anything is saved.
 *  -   The first checkpoint's own save at its start runs only for a new
 *      game: after a load the save is already there, and from the game as
 *      it is the place is unconfirmed (console B, 2026-11-11: a run on the
 *      HOME menu "saved" first).
 *  -   A failure leaves the game where it failed. Nothing here closes the
 *      game; the user (or the next run from the save) resumes from there.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRun_H
#define PokemonAutomation_Pokemon_StoryRun_H

#include <string>
#include <string_view>
#include "Common/Cpp/MoveOnlyFunction.h"
#include "Pokemon_AutoStoryStats.h"
#include "Pokemon_StoryCheckpointTypes.h"
#include "Pokemon_StoryOutcome.h"
#include "Pokemon_StorySegments.h"
#include "Pokemon_StoryPreflight.h"
#include "Pokemon_StoryStart.h"

namespace PokemonAutomation{
namespace Pokemon{


//  What a game lends the story run.
struct StoryGameHooks{
    //  Reads the console before anything else (see StoryPreflight). Without
    //  it the run logs that nothing was checked.
    MoveOnlyFunction<StoryPreflight()> preflight;
    //  Loads the last save: the game reset (from HOME or soft) and back in
    //  the overworld. A failed Outcome says why.
    Act load_save;
    //  Optional: after a load, "" when the save is where the checkpoint
    //  about to run starts, else why not.
    MoveOnlyFunction<std::string()> check_save_place;
    //  Optional: after a load and its place check, rereads what the save
    //  holds (FRLG: the bag's supplies).
    Act after_load;
    //  Optional: saves the game (rows that save at their end).
    Act save;
    EventLogger log;
    //  Optional: the step under way, for the video overlay.
    EventLogger status;
};

//  The line logged after any failure.
inline constexpr std::string_view STORY_LEFT_WHERE_FAILED = "AutoStory failed: the game stays where it failed.";

//  Gets the game to where checkpoint `checkpoint` starts, as `start` says.
//  First logs the checkpoint starting ("[Story] start: checkpoint 00.04b,
//  from its save") and runs the game's preflight; one that fails returns
//  StoryPreflight::FAILURE with nothing pressed. FROM_SAVE then always
//  loads the save (logging "[Story] 00.04b: loading the save"), checks its
//  place and rereads it, and sets the "save_loaded" flag.
Outcome prepare_story_start(std::string_view checkpoint, StoryStart start, StoryGameHooks& game, RunState& state);

//  The runner's hooks: reload is load_story_save(); save, log and status
//  are the game's. `game` must outlive them.
SegmentHooks story_segment_hooks(StoryGameHooks& game);

//  Ends a run: its counts into `stats`, then the failure line and an
//  OperationFailedException if it stopped short, else "AutoStory: done
//  through <last>." Returns that line, for the finish notification.
std::string finish_story_run(const SegmentReport& report, std::string_view last, AutoStoryStats& stats, StoryGameHooks& game);

//  Ends a verify run: the summary, and the failure line and an
//  OperationFailedException unless verified. Returns the summary.
std::string finish_story_verify(
    std::string_view checkpoint, const VerifyPolicy& policy, const VerifyReport& report, StoryGameHooks& game
);

//  The failure line, for a game whose run threw before the story run saw it.
void log_story_failure(StoryGameHooks& game);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
