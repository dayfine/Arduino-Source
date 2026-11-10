/*  Story Range Options
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <utility>
#include "Pokemon_StoryRangeOptions.h"

namespace PokemonAutomation{
namespace Pokemon{


StringSelectDatabase story_checkpoint_database(std::span<const std::string> labels){
    StringSelectDatabase database;
    for (size_t c = 0; c < labels.size(); c++){
        database.add_entry(StringSelectEntry(std::to_string(c), labels[c]));
    }
    return database;
}


StoryRangeOptions::StoryRangeOptions(std::string label, const StringSelectDatabase& checkpoints, LockMode end_lock)
    : GroupOption(std::move(label), LockMode::LOCK_WHILE_RUNNING)
    , START_CHECKPOINT(
        "<b>Start checkpoint:</b><br>The program starts from this checkpoint's save.",
        checkpoints,
        LockMode::LOCK_WHILE_RUNNING,
        0
    )
    , END_CHECKPOINT(
        "<b>End checkpoint:</b><br>The program stops after this checkpoint.",
        checkpoints,
        end_lock,
        checkpoints.size() == 0 ? 0 : checkpoints.size() - 1
    )
{
    PA_ADD_OPTION(START_CHECKPOINT);
    PA_ADD_OPTION(END_CHECKPOINT);
}

size_t StoryRangeOptions::start_index() const{
    return START_CHECKPOINT.index();
}

size_t StoryRangeOptions::end_index() const{
    return END_CHECKPOINT.index();
}

std::string StoryRangeOptions::check_validity() const{
    if (start_index() > end_index()){
        return "The start checkpoint comes after the end checkpoint.";
    }
    return GroupOption::check_validity();
}



} // namespace Pokemon
} // namespace PokemonAutomation
