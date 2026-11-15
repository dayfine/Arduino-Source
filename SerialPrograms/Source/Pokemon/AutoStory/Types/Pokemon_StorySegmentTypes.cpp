/*  Story Segment Types
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_StorySegmentTypes.h"

namespace PokemonAutomation{
namespace Pokemon{


void record_story_counts(const StoryRunCounts& counts, AutoStoryStats& stats){
    stats.m_segments_started += counts.segments;
    stats.m_checkpoints_started += counts.checkpoints;
    stats.m_resets += counts.resets;
}

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
    return StoryRunCounts{.segments = 1, .checkpoints = runs.size(), .resets = reloads};
}



} // namespace Pokemon
} // namespace PokemonAutomation
