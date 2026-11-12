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
//  the table ends. 15.01 starts at Celadon's nurse, after Erika
//  (14.04); 12.02 ends at Fuchsia, and the rows between are authored separately. An id leaves
//  this list once the rows before it are in.
inline bool starts_after_unauthored_rows(std::string_view id){
    return id == "15.01";
}


} // namespace PokemonFRLG
} // namespace NintendoSwitch
} // namespace PokemonAutomation
#endif
