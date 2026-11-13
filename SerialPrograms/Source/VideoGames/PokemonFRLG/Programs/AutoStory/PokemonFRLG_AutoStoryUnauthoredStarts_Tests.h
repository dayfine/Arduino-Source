/*  AutoStory Unauthored Starts
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  Which checkpoints start from their own declared pose in the tests.
 *
 */

#ifndef PokemonAutomation_PokemonFRLG_AutoStoryUnauthoredStarts_Tests_H
#define PokemonAutomation_PokemonFRLG_AutoStoryUnauthoredStarts_Tests_H

#include <string_view>

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonFRLG{


//  Whether checkpoint `id` was authored before the rows that lead up to it:
//  it starts from its own declared pose, not where the row before it in
//  the story's order (STORY_ORDER in PokemonFRLG_AutoStoryTable.cpp) ends.
//  17.01 starts at Saffron's Center nurse, where no row before it in the
//  order ends (Silph Co.'s 16.01 ends on a pad on 5F; the rest of Silph Co.
//  and Sabrina are not authored). 14.01 starts outside the Pokemon Mansion,
//  after its Escape Rope (13.04), which is not authored. An id leaves this
//  list once the rows before it are in.
inline bool starts_after_unauthored_rows(std::string_view id){
    return id == "17.01" || id == "14.01";
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
