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
