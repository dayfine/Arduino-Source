/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row){
    if (row.steps.empty()){
        return false;
    }
    const Pokemon::StoryStep& first = row.steps.front();
    return first.kind == Pokemon::StoryStepKind::GAME && (first.action == "new_game_intro" || first.action == "confirm_new_game");
}



} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
