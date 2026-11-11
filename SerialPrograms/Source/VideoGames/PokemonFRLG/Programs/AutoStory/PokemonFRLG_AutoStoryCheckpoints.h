/*  AutoStory Checkpoints
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  FRLG's story checkpoint, and the fixed routes, wall turns and mart
 *  orders its rows are built from.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryCheckpoints_H
#define PokemonAutomation_PokemonFRLG_AutoStoryCheckpoints_H

#include <functional>
#include <optional>
#include <string>
#include <vector>
#include "NintendoSwitch/Controllers/Procon/NintendoSwitch_ProController.h"
#include "NintendoSwitch/NintendoSwitch_ConsoleHandle.h"
#include "Pokemon/AutoStory/Pokemon_StoryActions.h"
#include "Pokemon/AutoStory/Pokemon_StoryEvents.h"
#include "Pokemon/AutoStory/Pokemon_StorySegments.h"
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldMap.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldRoute.h"
#include "VideoGames/PokemonFRLG/Overworld/PokemonFRLG_OverworldWalk.h"
#include "PokemonFRLG_AutoStoryActions.h"
#include "PokemonFRLG_AutoStoryBattle.h"
#include "PokemonFRLG_AutoStoryStartFrames.h"
#include "PokemonFRLG_AutoStoryStepInterrupts.h"
#include "PokemonFRLG_AutoStoryStepTable.h"
#include "PokemonFRLG_AutoStorySaveChecks.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  An upstream RNG program the user may run part-way through the story.
//  Each is off unless its option is on.
enum class StoryHandoff{
    NONE,
    //  The checkpoint's new game stops on the player's keyboard, the name
    //  typed and OK not pressed, which is where SID Helper starts.
    SID_HELPER,
    //  The checkpoint saves south of the table, facing the starter's ball,
    //  which is where Starter RNG starts. With the handoff off, it runs on
    //  into the next checkpoint as one, as the story did before the split.
    STARTER_RNG,
    //  The checkpoint saves beside the Magikarp salesman, facing him, which
    //  is where Gift RNG starts.
    GIFT_RNG,
};

//  A shared checkpoint with what FRLG adds to it.
struct StoryCheckpoint : public Pokemon::StoryCheckpoint{
    //  The healing items at its start, for a run that loads its save.
    Supplies supplies;
    //  The START menu at its end, when it saves.
    StartMenu menu = StartMenu::FULL;
    //  Starts from the game as another program left it, loading no save:
    //  the way back into the story after an RNG program. A run that starts
    //  before it passes over it.
    bool live_start = false;
    //  The RNG program the user may run at this checkpoint.
    StoryHandoff handoff = StoryHandoff::NONE;
};

//  The handoffs a run stops for (AutoStory's options).
struct StoryHandoffs{
    bool sid_helper = false;
    bool starter_rng = false;
    bool gift_rng = false;
};

//  What a run from table row `first` to `last` does.
struct StoryRunPlan{
    //  The checkpoints to run, in order.
    std::vector<StoryCheckpoint> rows;
    //  NONE: the run ends after `rows`. SID_HELPER: `rows` is empty; the run
    //  plays the new game to the player's keyboard and stops there.
    //  STARTER_RNG: the run stops after `rows`, the last of which saves at
    //  the starter's ball. GIFT_RNG: likewise, beside the Magikarp salesman;
    //  with `rows` empty, the run started at 00.12b and takes Gift RNG from
    //  the save beside the salesman, where the row before `first` ends.
    StoryHandoff stop = StoryHandoff::NONE;
};

//  The rows of `checkpoints` from `first` to `last`, both inclusive, as a
//  run with `handoffs` takes them: live-start rows only when the run
//  starts there (00.12b with Gift RNG's handoff on starts at the handoff
//  instead), a run that reaches a handoff that is on stops for it, and
//  a STARTER_RNG row whose handoff is off is joined with the row after it,
//  under its own id.
StoryRunPlan plan_story_run(
    const std::vector<StoryCheckpoint>& checkpoints, size_t first, size_t last, const StoryHandoffs& handoffs
);
//  A way the player at `tile` can turn to without stepping: a wall there,
//  with no one standing in it. West and east first: walking north or south
//  into a sign would read it.
std::optional<Direction> wall_beside(const MapData& map, const TilePos& tile);



} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
