/*  Story Files
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Common/Cpp/Exceptions.h"
#include "Pokemon_StoryFiles.h"

namespace PokemonAutomation{
namespace Pokemon{
namespace detail{


void add_story_row(
    std::string_view id, const StoryRowExecution& execution, RowSave start,
    std::optional<std::string_view> previous, std::string_view source, StoryRows& rows
){
    if (execution.reload != StoryReload::NEVER){
        if (start == RowSave::WITHOUT_SAVE){
            throw ParseException(std::format("{}: reload_when_chained on a checkpoint with no save to reload", source));
        }
        if (!previous || !rows.autosaves.contains(*previous)){
            throw ParseException(std::format(
                "{}: reload_when_chained after a checkpoint that does not end on the game's autosave: the reload would load an older save",
                source
            ));
        }
        rows.reload_when_chained.emplace(id);
    }
    if (execution.reload == StoryReload::ALWAYS){
        rows.reload_always.emplace(id);
    }
    switch (execution.save){
    case StoryRowSave::NONE:
        break;
    case StoryRowSave::AUTOSAVE:
        rows.autosaves.emplace(id);
        break;
    case StoryRowSave::UNVERIFIED:
        rows.unverified_autosaves.emplace(id);
        break;
    }
}



} // namespace detail
} // namespace Pokemon
} // namespace PokemonAutomation
