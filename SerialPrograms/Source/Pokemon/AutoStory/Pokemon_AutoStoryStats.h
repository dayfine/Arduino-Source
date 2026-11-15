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


//  Stats shared by the AutoStory programs of every game.
struct AutoStoryStats : public StatsTracker{
    AutoStoryStats()
        : m_runs(m_stats["Runs"])
        , m_segments_started(m_stats["Segments Started"])
        , m_checkpoints_started(m_stats["Checkpoints Started"])
        , m_resets(m_stats["Resets"])
    {
        m_display_order.emplace_back("Runs");
        m_display_order.emplace_back("Segments Started");
        m_display_order.emplace_back("Checkpoints Started");
        m_display_order.emplace_back("Resets");

        //  Labels used before the rename: saved totals carry over.
        m_aliases["Segment"] = "Segments Started";
        m_aliases["Checkpoint"] = "Checkpoints Started";
        m_aliases["Reset"] = "Resets";
    }
    std::atomic<uint64_t>& m_runs;
    std::atomic<uint64_t>& m_segments_started;
    std::atomic<uint64_t>& m_checkpoints_started;
    std::atomic<uint64_t>& m_resets;
};



}  // namespace Pokemon
}  // namespace PokemonAutomation
#endif
