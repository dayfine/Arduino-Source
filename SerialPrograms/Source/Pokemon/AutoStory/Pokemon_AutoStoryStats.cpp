/*  AutoStory Stats
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#include "Pokemon_AutoStoryStats.h"

namespace PokemonAutomation{
namespace Pokemon{


void record_story_counts(AutoStoryStats& stats, const StoryRunCounts& counts){
    stats.m_segment += counts.segments;
    stats.m_checkpoint += counts.checkpoints;
    stats.m_reset += counts.resets;
}



} // namespace Pokemon
} // namespace PokemonAutomation
