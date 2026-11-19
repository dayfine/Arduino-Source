/*  PokemonLZA AutoStory - Table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include <string>
#include <string_view>
#include "PokemonLZA_AutoStoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonLZA{


namespace{

//  Whether `row`'s first step is the GAME action `action`.
bool starts_with_game_action(const Pokemon::StoryCheckpoint& row, std::string_view action){
    if (row.steps.empty()){
        return false;
    }
    const Pokemon::StoryStep& first = row.steps.front();
    return first.kind == Pokemon::StoryStepKind::GAME && first.action == action;
}

} // namespace

bool story_row_starts_without_save(const Pokemon::StoryCheckpoint& row){
    return starts_with_game_action(row, "new_game_intro") || starts_with_game_action(row, "confirm_new_game");
}

bool story_row_makes_new_game_choices(const Pokemon::StoryCheckpoint& row){
    return starts_with_game_action(row, "new_game_intro");
}




} // namespace PokemonLZA
} // namespace NintendoSwitch
} // namespace PokemonAutomation
