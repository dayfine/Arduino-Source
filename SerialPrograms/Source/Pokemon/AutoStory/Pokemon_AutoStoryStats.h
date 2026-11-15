/*  AutoStory Stats
 *
 *  From: https://github.com/PokemonAutomation/
 *
 */

#ifndef PokemonAutomation_Pokemon_AutoStoryStats_H
#define PokemonAutomation_Pokemon_AutoStoryStats_H

#include <atomic>
#include <cstdint>
#include "CommonFramework/ProgramStats/StatsTracking.h"

namespace PokemonAutomation{
namespace Pokemon{


//  Scarlet/Violet's AutoStory stats: saved checkpoints, segments (story
//  chapters) started and game resets. The names are the keys of the saved
//  stats; do not rename them.
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

//  The stats of a story engine's AutoStory: program runs, checkpoints passed
//  and game resets to a checkpoint's save.
struct StoryRunStats : public StatsTracker{
    StoryRunStats()
        : m_runs(m_stats["Runs"])
        , m_checkpoints_passed(m_stats["Checkpoints Passed"])
        , m_resets(m_stats["Resets"])
    {
        m_display_order.emplace_back("Runs");
        m_display_order.emplace_back("Checkpoints Passed");
        m_display_order.emplace_back("Resets");
    }
    std::atomic<uint64_t>& m_runs;
    std::atomic<uint64_t>& m_checkpoints_passed;
    std::atomic<uint64_t>& m_resets;
};


} // namespace Pokemon
} // namespace PokemonAutomation
#endif
