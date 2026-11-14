/*  BDSP AutoStory - Story table
 *
 *  From: https://github.com/PokemonAutomation/
 *
 *  The story from a new game to Floaroma, 00.00 to 01.11, as rows of the
 *  shared story steps: plain data loaded from the embedded authored plans.
 *
 */

#include "PokemonBDSP_AutoStory_StoryData.h"
#include "PokemonBDSP_AutoStory_StoryTable.h"

namespace PokemonAutomation{
namespace NintendoSwitch{
namespace PokemonBDSP{


const std::vector<StoryCheckpoint>& story_checkpoints(){
    static const std::vector<StoryCheckpoint> checkpoints = load_bdsp_story({}).checkpoints;
    return checkpoints;
}


} // namespace PokemonBDSP
} // namespace NintendoSwitch
} // namespace PokemonAutomation
