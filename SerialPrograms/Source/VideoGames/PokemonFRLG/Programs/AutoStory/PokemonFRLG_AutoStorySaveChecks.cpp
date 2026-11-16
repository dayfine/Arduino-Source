/*  AutoStory Save Checks
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <format>
#include "Pokemon/AutoStory/Pokemon_StoryStepExecutor.h"
#include "PokemonFRLG_AutoStorySaveChecks.h"
#include "PokemonFRLG_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


std::string checkpoint_end_map(const std::string& id){
    const std::vector<StoryCheckpoint>& table = story_checkpoints();
    for (size_t c = 0; c + 1 < table.size(); c++){
        if (table[c].id == id && table[c + 1].start){
            return Pokemon::grid_pose_of(*table[c + 1].start).map;
        }
    }
    return "";
}

std::string save_end_error(const std::string& end_map, const std::string& place){
    const std::optional<std::string> expected = save_card_place(end_map);
    if (!expected || place == *expected){
        return "";
    }
    if (place.empty()){
        return std::format("The save card's place can't be read, so the end in {} ({}) isn't confirmed.", *expected, end_map);
    }
    return std::format("The save card names {}, but the checkpoint ends in {} ({}).", place, *expected, end_map);
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
