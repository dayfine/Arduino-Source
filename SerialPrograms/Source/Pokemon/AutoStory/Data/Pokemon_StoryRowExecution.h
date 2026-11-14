/*  Story Row Execution
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The execution fields of a checkpoint file (its JSON "execution") that
 *  every game reads the same way, and the sets of rows they mark: which
 *  rows end on the game's own autosave, which a run reloads before, and
 *  which are for lab runs only, and a game's story: its checkpoints and
 *  those rows. The game reads its other execution fields itself
 *  (Pokemon_AuthoredCheckpointJson.h); Pokemon_StoryFiles.h loads a story
 *  from its files.
 *
 */

#ifndef PokemonAutomation_Pokemon_StoryRowExecution_H
#define PokemonAutomation_Pokemon_StoryRowExecution_H

#include <functional>
#include <initializer_list>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include "Common/Cpp/Json/JsonObject.h"

namespace PokemonAutomation{
namespace Pokemon{


//  When a run reloads the row's save before it, having played the row
//  before it too ("reload_when_chained").
enum class StoryReload{
    NEVER,          //  left out: the row runs on from where the row before left
    WHEN_CHAINED,   //  true: at plan boundaries (StoryRunPolicy::BoundaryReload)
    ALWAYS,         //  "always": even straight through; only its save starts its walk
};

//  What a checkpoint file's shared execution fields say.
struct StoryRowExecution{
    //  "save": "autosave": the game saves by itself at the row's end, so the
    //  program does not. "menu" or left out: the program saves, when the
    //  save policy lets it.
    bool autosave = false;
    StoryReload reload = StoryReload::NEVER;
    //  "dev_only": true: a lab walk the story does not need (developer
    //  mode lists and plays it, a user's run does not).
    bool dev_only = false;
};

//  The shared fields of `execution`: "save", "reload_when_chained" and
//  "dev_only", each optional. Throws ParseException naming `where` for a
//  malformed one, and for a field that is neither shared nor one of
//  `game_fields`, which the game reads itself.
StoryRowExecution read_story_row_execution(
    const JsonObject& execution, std::string_view where, std::initializer_list<std::string_view> game_fields
);


//  The rows of a story the shared execution fields mark, by id.
struct StoryRows{
    std::set<std::string, std::less<>> autosaves;
    //  WHEN_CHAINED and ALWAYS rows; each comes after an autosave row,
    //  whose autosave is its start save.
    std::set<std::string, std::less<>> reload_when_chained;
    //  The ALWAYS rows of reload_when_chained.
    std::set<std::string, std::less<>> reload_always;
    std::set<std::string, std::less<>> dev_only;
    //  The dev files that replaced a built-in checkpoint, as (file name,
    //  path) (Pokemon_StoryDevFiles.h).
    std::vector<std::pair<std::string, std::string>> dev_files;
};

//  A game's story: its checkpoints in the order it plays them, and the rows
//  their execution fields mark.
template <typename Checkpoint>
struct StoryData : StoryRows{
    std::vector<Checkpoint> checkpoints;
};

//  How a run plays its checkpoints: the attempts each gets, and whether it
//  reloads at the plan's boundaries.
struct StoryRunPolicy{
    //  Whether a run of several checkpoints reloads the save at a
    //  WHEN_CHAINED row (AT_PLAN_BOUNDARIES) or runs on from the state the
    //  row before left (STRAIGHT_THROUGH). ALWAYS rows reload either way, and
    //  a failed checkpoint's retries reload as usual.
    enum class BoundaryReload{
        AT_PLAN_BOUNDARIES,
        STRAIGHT_THROUGH,
    };
    size_t attempts_per_checkpoint = 1;
    BoundaryReload boundary_reload = BoundaryReload::AT_PLAN_BOUNDARIES;
};

//  Whether a run reloads the save before row `id` of `rows` when it reaches
//  it from the row before.
bool story_reloads_before(const StoryRows& rows, StoryRunPolicy::BoundaryReload boundary_reload, std::string_view id);



} // namespace Pokemon
} // namespace PokemonAutomation
#endif
