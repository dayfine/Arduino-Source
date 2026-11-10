/*  Story Segment Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StorySegmentTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


bool CheckpointRun::one_shot() const{
    return passed && attempts == 1;
}

size_t SegmentReport::one_shot_passes() const{
    size_t count = 0;
    for (const CheckpointRun& run : runs){
        count += run.one_shot() ? 1 : 0;
    }
    return count;
}

StoryRunCounts SegmentReport::story_counts() const{
    StoryRunCounts counts{.segments = 1, .checkpoints = 0, .resets = reloads};
    for (const CheckpointRun& run : runs){
        if (run.passed){
            counts.checkpoints++;
        }
    }
    return counts;
}



} // namespace Pokemon
} // namespace PokemonAutomation
