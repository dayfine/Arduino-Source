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
//  14.03 starts at Cinnabar's Center nurse, where no row before it in the
//  order ends (the Mansion's 13.04 ends outside, by an Escape Rope, and
//  Blaine's rows 14.01 to 14.02b, which heal at that Center, are not in
//  yet). An id leaves this list once the rows before it are in.
inline bool starts_after_unauthored_rows(std::string_view id){
    return id == "14.03";
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
