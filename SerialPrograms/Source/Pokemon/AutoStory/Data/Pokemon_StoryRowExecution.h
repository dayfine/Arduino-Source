/*  Story Row Execution
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The execution fields every game's checkpoint file shares.
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
    WHEN_CHAINED,   //  true: at plan boundaries
};

//  What a row's "save" says about the game's own save at its end.
enum class StoryRowSave{
    NONE,           //  left out: the game does not save at the row's end
    AUTOSAVE,       //  "autosave": a run's log showed the game autosave there
    UNVERIFIED,     //  "unverified": believed to autosave there, not yet seen in a run's log; a reload does not count on it
};

//  What a checkpoint file's shared execution fields say.
struct StoryRowExecution{
    StoryRowSave save = StoryRowSave::NONE;
    StoryReload reload = StoryReload::NEVER;
};

//  The shared fields of `execution`. Throws ParseException naming `where`
//  for a malformed field or one that is neither shared nor in `game_fields`.
StoryRowExecution read_story_row_execution(
    const JsonObject& execution, std::string_view where, std::initializer_list<std::string_view> game_fields
);


//  The rows of a story the shared execution fields mark, by id.
struct StoryRows{
    std::set<std::string, std::less<>> autosaves = {};
    //  The StoryRowSave::UNVERIFIED rows.
    std::set<std::string, std::less<>> unverified_autosaves = {};
    //  The WHEN_CHAINED rows; each comes after an autosave row.
    std::set<std::string, std::less<>> reload_when_chained = {};
};

//  A game's story: its checkpoints in the order it plays them, and the rows
//  their execution fields mark.
template <typename Checkpoint>
struct StoryData : StoryRows{
    std::vector<Checkpoint> checkpoints = {};
};




} // namespace Pokemon
} // namespace PokemonAutomation
#endif
