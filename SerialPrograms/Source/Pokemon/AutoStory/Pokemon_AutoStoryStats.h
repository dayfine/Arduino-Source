/*  AutoStory Stats
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStoryStats_H
#define PokemonAutomation_Pokemon_AutoStoryStats_H

#include <cstddef>
#include "CommonFramework/ProgramStats/StatsTracking.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Saved checkpoints, segments started and game resets of an AutoStory run.
//  The names are the keys of the saved stats; do not rename them.
struct AutoStoryStats : public StatsTracker{
    AutoStoryStats()
        : m_checkpoint(m_stats["Checkpoint"])
        , m_segment(m_stats["Segment"])
        , m_reset(m_stats["Reset"])
    {
        m_display_order.emplace_back("Checkpoint");
        m_display_order.emplace_back("Segment");
        m_display_order.emplace_back("Reset");
    }
    std::atomic<uint64_t>& m_checkpoint;
    std::atomic<uint64_t>& m_segment;
    std::atomic<uint64_t>& m_reset;
};


//  What a story run adds to the program's stats.
struct StoryRunCounts{
    size_t segments = 0;
    //  Checkpoints passed (and saved at).
    size_t checkpoints = 0;
    //  Game resets or reloads after failed attempts.
    size_t resets = 0;
};

//  Adds `counts` to the shared AutoStory stats.
void record_story_counts(const StoryRunCounts& counts, AutoStoryStats& stats);


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
